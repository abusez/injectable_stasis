#include "../../pch.hpp"
#include "Minecraft.hpp"
#include "../../SDK/sdk.hpp"
#include <cstdio>

jmethodID Minecraft::s_leftClick  = nullptr;
jmethodID Minecraft::s_rightClick = nullptr;
bool      Minecraft::s_cached     = false;

// Cached java.lang.reflect.Field global-refs (survive across calls, work for any obfuscation)
static jobject s_playerField  = nullptr; // Field for thePlayer
static jobject s_worldField   = nullptr; // Field for theWorld
static jobject s_screenField  = nullptr; // Field for currentScreen

static Minecraft* s_instance = nullptr;

// ---- Logging ------------------------------------------------------------
static void McLog(const char* fmt, ...) {
    char buf[512]; va_list a; va_start(a,fmt); vsnprintf(buf,sizeof(buf),fmt,a); va_end(a);
    OutputDebugStringA("[MC] "); OutputDebugStringA(buf); OutputDebugStringA("\n");
    char path[MAX_PATH]; GetTempPathA(MAX_PATH, path); strcat(path,"stasis_debug.log");
    if (FILE* f=fopen(path,"a")) { fputs("[MC] ",f); fputs(buf,f); fputc('\n',f); fclose(f); }
}

// ---- Reflection helpers -------------------------------------------------

// Returns the java.lang.Class of an object.
static jobject getClass(JNIEnv* env, jobject obj) {
    jclass objCls = env->FindClass("java/lang/Object");
    jmethodID mid = env->GetMethodID(objCls, "getClass", "()Ljava/lang/Class;");
    jobject c = env->CallObjectMethod(obj, mid);
    SDK::ClearException(env);
    return c;
}

// Scans all declared fields of `cls` via reflection.
// For each non-null, non-primitive field value from `instance`, calls `cb(fieldName, typeName, fieldObj, value)`.
// field and value are local refs; do NOT delete them — the callback owns their lifetime.
static void scanFields(JNIEnv* env, jobject cls, jobject instance,
    std::function<bool(const std::string& fname, const std::string& tname, jobject field, jobject value)> cb)
{
    jclass classCls = env->FindClass("java/lang/Class");
    jmethodID getDeclFlds = env->GetMethodID(classCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
    jobjectArray flds = (jobjectArray)env->CallObjectMethod(cls, getDeclFlds);
    SDK::ClearException(env);
    if (!flds) return;

    jclass fldCls = env->FindClass("java/lang/reflect/Field");
    jmethodID setAcc  = env->GetMethodID(fldCls, "setAccessible", "(Z)V");
    jmethodID fGet    = env->GetMethodID(fldCls, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    jmethodID fName   = env->GetMethodID(fldCls, "getName", "()Ljava/lang/String;");
    jmethodID fType   = env->GetMethodID(fldCls, "getType", "()Ljava/lang/Class;");
    jmethodID cName   = env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
    jmethodID isPrim  = env->GetMethodID(classCls, "isPrimitive", "()Z");

    int len = env->GetArrayLength(flds);
    for (int i = 0; i < len; i++) {
        jobject f = env->GetObjectArrayElement(flds, i);
        env->CallVoidMethod(f, setAcc, JNI_TRUE); SDK::ClearException(env);

        jobject ftype = env->CallObjectMethod(f, fType); SDK::ClearException(env);
        if (ftype && env->CallBooleanMethod(ftype, isPrim) == JNI_TRUE) {
            env->DeleteLocalRef(f); env->DeleteLocalRef(ftype); continue;
        }

        jobject val = env->CallObjectMethod(f, fGet, instance); SDK::ClearException(env);
        if (!val) { env->DeleteLocalRef(f); if (ftype) env->DeleteLocalRef(ftype); continue; }

        jstring fn  = (jstring)env->CallObjectMethod(f, fName); SDK::ClearException(env);
        jstring tn  = ftype ? (jstring)env->CallObjectMethod(ftype, cName) : nullptr; SDK::ClearException(env);
        std::string fnStr = fn ? SDK::JStringToStr(env, fn) : "?";
        std::string tnStr = tn ? SDK::JStringToStr(env, tn) : "?";
        if (fn) env->DeleteLocalRef(fn);
        if (tn) env->DeleteLocalRef(tn);
        if (ftype) env->DeleteLocalRef(ftype);

        bool stop = cb(fnStr, tnStr, f, val);
        env->DeleteLocalRef(val);
        env->DeleteLocalRef(f);
        if (stop) break;
    }
    env->DeleteLocalRef(flds);
}

// One-time scan of the Minecraft class to find thePlayer, theWorld, currentScreen.
// Results are cached as GlobalRef java.lang.reflect.Field objects.
static void discoverMinecraftFields(JNIEnv* env, jobject mcObj) {
    if (s_playerField) return; // already done

    McLog("Scanning ave (Minecraft) fields via reflection...");
    jobject mcCls = getClass(env, mcObj);
    if (!mcCls) { McLog("Failed to get mc class"); return; }

    // Match by type name — avoids IsInstanceOf failures when our "sv"/"bew" guess
    // doesn't match Badlion's actual class hierarchy mapping.
    // Known player type names across 1.8.9 builds:
    static const char* kPlayerTypes[] = { "bew", "bip", "EntityPlayer", "EntityClientPlayerMP", nullptr };
    // Known world type names:
    static const char* kWorldTypes[]  = { "bdb", "bkr", "bkq", "WorldClient", nullptr };
    // Screen type is aqh (confirmed by log):
    static const char* kScreenTypes[] = { "aqh", "GuiScreen", nullptr };

    auto matchesAny = [](const std::string& t, const char** list) {
        for (int i = 0; list[i]; i++) if (t == list[i]) return true;
        return false;
    };

    scanFields(env, mcCls, mcObj,
        [&](const std::string& fname, const std::string& tname, jobject field, jobject value) -> bool {
            McLog("  ave.%s :: %s", fname.c_str(), tname.c_str());

            if (!s_playerField && matchesAny(tname, kPlayerTypes)) {
                s_playerField = env->NewGlobalRef(field);
                McLog("  ^ selected as thePlayer (type=%s)", tname.c_str());
            }
            if (!s_worldField && matchesAny(tname, kWorldTypes)) {
                s_worldField = env->NewGlobalRef(field);
                McLog("  ^ selected as theWorld (type=%s)", tname.c_str());
            }
            if (!s_screenField && matchesAny(tname, kScreenTypes)) {
                s_screenField = env->NewGlobalRef(field);
                McLog("  ^ selected as currentScreen (type=%s)", tname.c_str());
            }

            return false;
        });

    env->DeleteLocalRef(mcCls);
    McLog("Scan complete. playerField=%s screenField=%s",
          s_playerField ? "found" : "NOT FOUND",
          s_screenField ? "found" : "NOT FOUND");
}

// ---- Minecraft::get() ---------------------------------------------------

Minecraft* Minecraft::get() {
    JNIEnv* env = SDK::GetEnv();
    if (!env) return nullptr;
    jobject obj = SDK::GetMinecraftObject(env);
    if (!obj) return nullptr;

    if (!s_instance) s_instance = new Minecraft(env, obj);
    else { s_instance->m_env = env; s_instance->m_obj = obj; }

    s_instance->ensureCached();
    return s_instance;
}

void Minecraft::ensureCached() {
    discoverMinecraftFields(m_env, m_obj);

    if (s_cached) return;
    jclass mc = m_env->FindClass("ave");
    if (!mc) { SDK::ClearException(m_env); return; }
    s_leftClick  = m_env->GetMethodID(mc, "ai", "()V"); SDK::ClearException(m_env);
    s_rightClick = m_env->GetMethodID(mc, "ah", "()V"); SDK::ClearException(m_env);
    s_cached = true;
}

// ---- Field access via cached reflection Field ---------------------------

static jobject getFieldValue(JNIEnv* env, jobject reflField, jobject instance) {
    if (!reflField || !instance) return nullptr;
    jclass fldCls = env->FindClass("java/lang/reflect/Field");
    jmethodID getMid = env->GetMethodID(fldCls, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    jobject val = env->CallObjectMethod(reflField, getMid, instance);
    SDK::ClearException(env);
    return val;
}

// ---- Public API ---------------------------------------------------------

jobject Minecraft::getPlayer() const {
    // The field is discovered in ensureCached via discoverMinecraftFields.
    // Use reflection Field.get() — works regardless of obfuscated type descriptor.
    return getFieldValue(m_env, s_playerField, m_obj);
}

jobject Minecraft::getWorld() const {
    return getFieldValue(m_env, s_worldField, m_obj);
}

jobject Minecraft::getMouseOver() const {
    jclass mc = m_env->FindClass("ave");
    if (!mc) { SDK::ClearException(m_env); return nullptr; }
    jfieldID fid = m_env->GetFieldID(mc, "i", "Lbmo;");
    SDK::ClearException(m_env);
    if (!fid) return nullptr;
    return m_env->GetObjectField(m_obj, fid);
}

bool Minecraft::isInGui() const {
    // currentScreen is null when in-game — confirmed GuiScreen class = aqh in Badlion.
    // The field is only non-null while a GUI is open, so s_screenField won't be
    // discovered in a world scan. Fall back to direct JNI with the known name.
    jclass mc = m_env->FindClass("ave");
    if (!mc) { SDK::ClearException(m_env); return false; }
    jfieldID fid = m_env->GetFieldID(mc, "r", "Laqh;");
    SDK::ClearException(m_env);
    if (!fid) return false;
    return m_env->GetObjectField(m_obj, fid) != nullptr;
}

void Minecraft::leftClick() {
    if (s_leftClick) m_env->CallVoidMethod(m_obj, s_leftClick);
    SDK::ClearException(m_env);
}

void Minecraft::rightClick() {
    if (s_rightClick) m_env->CallVoidMethod(m_obj, s_rightClick);
    SDK::ClearException(m_env);
}
