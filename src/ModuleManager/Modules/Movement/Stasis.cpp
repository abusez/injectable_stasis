#include "pch.hpp"
#include "Stasis.hpp"
#include "SDK/sdk.hpp"
#include "SDK/Minecraft/Minecraft.hpp"
#include <cstdarg>

// ---- Debug log ----------------------------------------------------------
// Writes to C:\stasis_debug.log and OutputDebugString simultaneously.
// Delete or comment out the fopen calls once the module is confirmed working.

static const char* LogPath() {
    static char path[MAX_PATH] = {};
    if (!path[0]) {
        char tmp[MAX_PATH];
        GetTempPathA(MAX_PATH, tmp);
        snprintf(path, MAX_PATH, "%sstasis_debug.log", tmp);
    }
    return path;
}

static void Log(const char* fmt, ...) {
    char buf[512];
    va_list args; va_start(args, fmt); vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);

    SYSTEMTIME st; GetLocalTime(&st);
    char line[600];
    snprintf(line, sizeof(line), "[%02d:%02d:%02d.%03d] [Stasis] %s\n",
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);

    OutputDebugStringA(line);
    if (FILE* f = fopen(LogPath(), "a")) { fputs(line, f); fclose(f); }
}

// ---- Badlion 1.8.9 confirmed field mapping (discovered via reflection dump) ---
//  Entity           (pk)   motionX="v"  motionY="w"  motionZ="x"
//  EntityLivingBase (wn)   hurtTime="bm"
//  EntityPlayer     (bew)  lastReportedPosX="bq" Y="br" Z="bs"
//                          movementInput="bA" (type wl, found in superclass wn)
//  MovementInput    (wl)   moveForward="e"  moveStrafing=scan(a/b/c/d)
// ------------------------------------------------------------------------------

Stasis::Stasis() : Module("Stasis", Category::Movement) {
    // 0 = disabled. 80 ticks (4 s) is the sweet spot for most servers.
    addInt("Pulse Ticks", 0, 200, 80);
}

void Stasis::onEnable() {
    m_cached  = false;
    m_pulsing = false;
    // Offset so the first pulse fires ~1 second after enable rather than after
    // the full interval. Without this, toggling resets the timer and the server
    // flags you before the first pulse ever happens.
    m_lastPulse = Clock::now() - std::chrono::milliseconds(3000);
    Log("onEnable() called — log path: %s", LogPath());
    SDK::PrintChat("[Stasis] ON");
}

void Stasis::onDisable() {
    Log("onDisable() called");
    SDK::PrintChat("[Stasis] OFF");
}

void Stasis::onRender() {
    // Draw field-ID status directly in-game so you can see what's working
    // without needing file access. Shown only when Stasis is enabled.
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float y = 50.f;
    char buf[128];

    auto row = [&](const char* label, bool ok) {
        snprintf(buf, sizeof(buf), "[Stasis] %-14s %s", label, ok ? "OK" : "FAIL");
        ImU32 col = ok ? IM_COL32(0, 230, 120, 255) : IM_COL32(255, 70, 70, 255);
        dl->AddText(ImVec2(4.f, y), col, buf);
        y += 14.f;
    };

    if (!m_cached) {
        dl->AddText(ImVec2(4.f, y), IM_COL32(255, 200, 0, 255), "[Stasis] caching...");
        return;
    }

    row("motionX",    m_motionX    != nullptr);
    row("motionY",    m_motionY    != nullptr);
    row("motionZ",    m_motionZ    != nullptr);
    row("hurtTime",   m_hurtTime   != nullptr);
    row("moveInput",  m_moveInput  != nullptr);
    row("moveForward",m_moveForward!= nullptr);
    row("moveStrafe", m_moveStrafe != nullptr);
    row("lastRptX",   m_lastRptX   != nullptr);
}

// Scan and log every field in a class and its superclasses.
// Useful for discovering correct obfuscated names when they're unknown.
static void dumpClassHierarchy(JNIEnv* env, jobject obj) {
    jclass objCls   = env->FindClass("java/lang/Object");
    jclass classCls = env->FindClass("java/lang/Class");
    jmethodID getClass    = env->GetMethodID(objCls,   "getClass",         "()Ljava/lang/Class;");
    jmethodID getDeclFlds = env->GetMethodID(classCls, "getDeclaredFields","()[Ljava/lang/reflect/Field;");
    jmethodID getSuperCls = env->GetMethodID(classCls, "getSuperclass",    "()Ljava/lang/Class;");
    jmethodID clsGetName  = env->GetMethodID(classCls, "getName",          "()Ljava/lang/String;");

    jclass fldCls   = env->FindClass("java/lang/reflect/Field");
    jmethodID setAcc  = env->GetMethodID(fldCls, "setAccessible","(Z)V");
    jmethodID fGet    = env->GetMethodID(fldCls, "get",  "(Ljava/lang/Object;)Ljava/lang/Object;");
    jmethodID fName   = env->GetMethodID(fldCls, "getName","()Ljava/lang/String;");
    jmethodID fType   = env->GetMethodID(fldCls, "getType","()Ljava/lang/Class;");
    jmethodID isPrim  = env->GetMethodID(classCls,"isPrimitive","()Z");

    jobject cls = env->CallObjectMethod(obj, getClass); SDK::ClearException(env);

    while (cls) {
        jstring cname = (jstring)env->CallObjectMethod(cls, clsGetName); SDK::ClearException(env);
        std::string cnStr = cname ? SDK::JStringToStr(env, cname) : "?";
        if (cname) env->DeleteLocalRef(cname);
        Log("=== Class: %s ===", cnStr.c_str());

        jobjectArray flds = (jobjectArray)env->CallObjectMethod(cls, getDeclFlds); SDK::ClearException(env);
        if (flds) {
            int len = env->GetArrayLength(flds);
            for (int i = 0; i < len; i++) {
                jobject f = env->GetObjectArrayElement(flds, i);
                env->CallVoidMethod(f, setAcc, JNI_TRUE); SDK::ClearException(env);

                jobject ft = env->CallObjectMethod(f, fType); SDK::ClearException(env);
                bool prim  = ft && env->CallBooleanMethod(ft, isPrim)==JNI_TRUE;
                jstring fn = (jstring)env->CallObjectMethod(f, fName); SDK::ClearException(env);
                jstring tn = ft ? (jstring)env->CallObjectMethod(ft, clsGetName) : nullptr; SDK::ClearException(env);
                std::string fnStr = fn ? SDK::JStringToStr(env,fn) : "?";
                std::string tnStr = tn ? SDK::JStringToStr(env,tn) : "?";
                if (fn) env->DeleteLocalRef(fn);
                if (tn) env->DeleteLocalRef(tn);

                if (prim) {
                    Log("  %s :: %s (primitive)", fnStr.c_str(), tnStr.c_str());
                } else {
                    jobject val = env->CallObjectMethod(f, fGet, obj); SDK::ClearException(env);
                    Log("  %s :: %s = %s", fnStr.c_str(), tnStr.c_str(), val ? "non-null" : "null");
                    if (val) env->DeleteLocalRef(val);
                }
                if (ft) env->DeleteLocalRef(ft);
                env->DeleteLocalRef(f);
            }
            env->DeleteLocalRef(flds);
        }

        jobject superCls = env->CallObjectMethod(cls, getSuperCls); SDK::ClearException(env);
        env->DeleteLocalRef(cls);
        cls = superCls;

        // Stop at java.lang.Object
        if (cls) {
            jstring sn = (jstring)env->CallObjectMethod(cls, clsGetName); SDK::ClearException(env);
            std::string snStr = sn ? SDK::JStringToStr(env,sn) : "";
            if (sn) env->DeleteLocalRef(sn);
            if (snStr == "java.lang.Object") { env->DeleteLocalRef(cls); break; }
        }
    }
}

static jobject player_for_cache = nullptr; // set before cacheIDs, used inside it

void Stasis::cacheIDs(JNIEnv* env) {
    if (m_cached) return;
    Log("caching JNI field IDs...");

    auto tryClass = [&](const char* name) -> jclass {
        jclass local = env->FindClass(name);
        SDK::ClearException(env);
        if (!local) { Log("  FindClass(%s) FAILED", name); return nullptr; }
        Log("  FindClass(%s) OK", name);
        return (jclass)env->NewGlobalRef(local);
    };
    auto tryField = [&](jclass cls, const char* clsName, const char* name, const char* sig) -> jfieldID {
        if (!cls) return nullptr;
        jfieldID fid = env->GetFieldID(cls, name, sig);
        SDK::ClearException(env);
        if (!fid) { Log("  GetFieldID(%s, %s, %s) FAILED", clsName, name, sig); return nullptr; }
        Log("  GetFieldID(%s, %s, %s) OK", clsName, name, sig);
        return fid;
    };

    // Entity (pk) — confirmed via reflection dump
    m_entityClass = tryClass("pk");
    m_motionX     = tryField(m_entityClass, "pk", "v", "D");
    m_motionY     = tryField(m_entityClass, "pk", "w", "D");
    m_motionZ     = tryField(m_entityClass, "pk", "x", "D");

    // EntityLivingBase (wn) — confirmed via reflection dump
    m_livingClass = tryClass("wn");
    m_hurtTime    = tryField(m_livingClass, "wn", "bm", "I");

    // EntityPlayer (bew) — confirmed via reflection
    m_playerClass = tryClass("bew");
    // movementInput: field bA type wl, found in wn superclass of bew
    m_moveInput   = tryField(m_playerClass, "bew", "bA", "Lwl;");
    // lastReportedPos: confirmed working
    m_lastRptX    = tryField(m_playerClass, "bew", "bq", "D");
    m_lastRptY    = tryField(m_playerClass, "bew", "br", "D");
    m_lastRptZ    = tryField(m_playerClass, "bew", "bs", "D");

    // MovementInput — get the actual object from the player, then scan its
    // declared float primitive fields in order: first = moveForward, second = moveStrafing.
    // This avoids guessing field names entirely.
    if (m_moveInput) {
        jobject mi = env->GetObjectField(player_for_cache, m_moveInput);
        SDK::ClearException(env);
        if (!mi) {
            Log("  movementInput object is null");
        } else {
            jclass classCls  = env->FindClass("java/lang/Class");
            jclass fieldCls  = env->FindClass("java/lang/reflect/Field");
            jmethodID getClsMid      = env->GetMethodID(env->FindClass("java/lang/Object"), "getClass", "()Ljava/lang/Class;");
            jmethodID getDeclFldsMid = env->GetMethodID(classCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
            jmethodID setAccMid      = env->GetMethodID(fieldCls, "setAccessible", "(Z)V");
            jmethodID fNameMid       = env->GetMethodID(fieldCls, "getName", "()Ljava/lang/String;");
            jmethodID fTypeMid       = env->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;");
            jmethodID cNameMid       = env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
            jmethodID isPrimMid      = env->GetMethodID(classCls, "isPrimitive", "()Z");

            jobject miCls = env->CallObjectMethod(mi, getClsMid);
            jobjectArray flds = (jobjectArray)env->CallObjectMethod(miCls, getDeclFldsMid);
            SDK::ClearException(env);

            int floatCount = 0;
            int len = flds ? env->GetArrayLength(flds) : 0;
            for (int i = 0; i < len && floatCount < 2; i++) {
                jobject f = env->GetObjectArrayElement(flds, i);
                env->CallVoidMethod(f, setAccMid, JNI_TRUE); SDK::ClearException(env);

                jobject ft   = env->CallObjectMethod(f, fTypeMid); SDK::ClearException(env);
                jstring tn   = ft ? (jstring)env->CallObjectMethod(ft, cNameMid) : nullptr;
                std::string typeStr = tn ? SDK::JStringToStr(env, tn) : "";
                if (tn) env->DeleteLocalRef(tn);

                bool prim = ft && env->CallBooleanMethod(ft, isPrimMid) == JNI_TRUE;
                if (ft) env->DeleteLocalRef(ft);

                if (prim && typeStr == "float") {
                    jstring fn = (jstring)env->CallObjectMethod(f, fNameMid);
                    std::string nameStr = fn ? SDK::JStringToStr(env, fn) : "?";
                    if (fn) env->DeleteLocalRef(fn);

                    // Get jfieldID from the actual mi class
                    jfieldID fid = env->GetFieldID((jclass)miCls, nameStr.c_str(), "F");
                    SDK::ClearException(env);
                    if (fid) {
                        if (floatCount == 0) { m_moveForward = fid; Log("  moveForward = wl.%s", nameStr.c_str()); }
                        else                 { m_moveStrafe  = fid; Log("  moveStrafe  = wl.%s", nameStr.c_str()); }
                        floatCount++;
                    }
                }
                env->DeleteLocalRef(f);
            }
            if (flds) env->DeleteLocalRef(flds);
            env->DeleteLocalRef(miCls);
            env->DeleteLocalRef(mi);

            if (floatCount < 2)
                Log("  WARNING: only found %d float fields in MovementInput", floatCount);
        }
    }

    Log("cache done — motionX=%s hurtTime=%s moveInput=%s lastRptX=%s",
        m_motionX ? "OK" : "FAIL",
        m_hurtTime ? "OK" : "FAIL",
        m_moveInput ? "OK" : "FAIL",
        m_lastRptX ? "OK" : "FAIL");

    m_cached = true;
}

void Stasis::onTick() {
    auto mc = Minecraft::get();
    if (!mc || !mc->isValid()) { Log("tick: mc invalid"); return; }
    if (mc->isInGui())         { return; } // silent — happens constantly in menus

    JNIEnv* env = SDK::GetEnv();
    if (!env) { Log("tick: env null"); return; }

    jobject player = mc->getPlayer();
    if (!player) { Log("tick: player null"); return; }

    // ---- Pulse gate ------------------------------------------------------
    // Every pulseTicks Minecraft ticks (50 ms each), suspend stasis for one
    // tick so the game sends a real C03 position packet to the server.
    // This resets the server's "player hasn't moved" anti-cheat timer.
    {
        int pulseTicks = getInt("Pulse Ticks");
        if (pulseTicks > 0) {
            auto now = Clock::now();
            auto interval = std::chrono::milliseconds(pulseTicks * 50); // 50 ms per MC tick

            if (!m_pulsing && (now - m_lastPulse) >= interval) {
                // Time for a pulse — open the gate for 1 tick (50 ms)
                m_pulsing  = true;
                m_pulseEnd = now + std::chrono::milliseconds(50);
            }

            if (m_pulsing) {
                if (now < m_pulseEnd) {
                    // During pulse: let motion and packets through normally
                    return;
                }
                // Pulse over — resume stasis, reset timer
                m_pulsing   = false;
                m_lastPulse = now;
            }
        }
    }

    // Dump the full class hierarchy once so we can see all real field names.
    static bool s_dumped = false;
    if (!s_dumped) {
        Log("=== Player class hierarchy dump ===");
        dumpClassHierarchy(env, player);
        Log("=== End dump ===");
        s_dumped = true;
    }

    player_for_cache = player; // used inside cacheIDs for MovementInput scan
    cacheIDs(env);

    // hurtTime check
    if (m_livingClass && m_hurtTime) {
        jint ht = env->GetIntField(player, m_hurtTime);
        SDK::ClearException(env);
        if (ht != 0) { return; } // being knocked back — let physics run
    }

    // Zero motion
    if (m_entityClass && m_motionX) {
        env->SetDoubleField(player, m_motionX, 0.0);
        env->SetDoubleField(player, m_motionY, 0.0);
        env->SetDoubleField(player, m_motionZ, 0.0);
        SDK::ClearException(env);
    } else {
        Log("tick: motion fields unavailable — freeze not working");
    }

    // Zero movement input
    if (m_playerClass && m_moveInput) {
        jobject mi = env->GetObjectField(player, m_moveInput); SDK::ClearException(env);
        if (mi) {
            if (m_moveForward) { env->SetFloatField(mi, m_moveForward, 0.f); SDK::ClearException(env); }
            if (m_moveStrafe)  { env->SetFloatField(mi, m_moveStrafe,  0.f); SDK::ClearException(env); }
            env->DeleteLocalRef(mi);
        }
    }

    // Suppress position packets by keeping lastReportedPos == currentPos
    if (m_playerClass && m_lastRptX && m_entityClass) {
        // posX/Y/Z in pk: s/t/u (same field names, different class from vanilla ps)
        jfieldID pxFid = env->GetFieldID(m_entityClass, "s", "D"); SDK::ClearException(env);
        jfieldID pyFid = env->GetFieldID(m_entityClass, "t", "D"); SDK::ClearException(env);
        jfieldID pzFid = env->GetFieldID(m_entityClass, "u", "D"); SDK::ClearException(env);
        if (pxFid && pyFid && pzFid) {
            jdouble px = env->GetDoubleField(player, pxFid); SDK::ClearException(env);
            jdouble py = env->GetDoubleField(player, pyFid); SDK::ClearException(env);
            jdouble pz = env->GetDoubleField(player, pzFid); SDK::ClearException(env);
            env->SetDoubleField(player, m_lastRptX, px); SDK::ClearException(env);
            env->SetDoubleField(player, m_lastRptY, py); SDK::ClearException(env);
            env->SetDoubleField(player, m_lastRptZ, pz); SDK::ClearException(env);
        }
    }
}
