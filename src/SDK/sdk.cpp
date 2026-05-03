#include "../pch.hpp"
#include "sdk.hpp"

namespace SDK {

static JavaVM* s_vm = nullptr;

bool Init() {
    jsize count = 0;
    jint res = JNI_GetCreatedJavaVMs(&s_vm, 1, &count);
    return res == JNI_OK && count > 0 && s_vm != nullptr;
}

void Shutdown() {
    if (s_vm)
        s_vm->DetachCurrentThread();
}

JavaVM* GetVM() { return s_vm; }

JNIEnv* GetEnv() {
    if (!s_vm) return nullptr;

    JNIEnv* env = nullptr;
    jint res = s_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

    if (res == JNI_EDETACHED)
        s_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);

    return env;
}

std::string JStringToStr(JNIEnv* env, jstring str) {
    if (!str) return "";
    const char* chars = env->GetStringUTFChars(str, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(str, chars);
    return result;
}

jstring StrToJString(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}

void ClearException(JNIEnv* env) {
    if (env && env->ExceptionCheck())
        env->ExceptionClear();
}

bool CheckException(JNIEnv* env) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

jobject GetMinecraftObject(JNIEnv* env) {
    // Minecraft 1.8.9 obfuscated class: "ave"
    // Update this if targeting a different version (check with jadx/cfr on the jar)
    jclass mcClass = env->FindClass("ave");
    if (!mcClass) { ClearException(env); return nullptr; }

    // Static getMinecraft() method — obfuscated "A" returns Lave; in 1.8.9
    jmethodID getMc = env->GetStaticMethodID(mcClass, "A", "()Lave;");
    if (!getMc) { ClearException(env); return nullptr; }

    return env->CallStaticObjectMethod(mcClass, getMc);
}

// ---- PrintChat ----------------------------------------------------------

static const char* LogPath() {
    static char path[MAX_PATH] = {};
    if (!path[0]) {
        char tmp[MAX_PATH];
        GetTempPathA(MAX_PATH, tmp);
        snprintf(path, MAX_PATH, "%sstasis_debug.log", tmp);
    }
    return path;
}

static void DebugLog(const char* line) {
    OutputDebugStringA(line); OutputDebugStringA("\n");
    if (FILE* f = fopen(LogPath(), "a")) { fputs(line, f); fputc('\n', f); fclose(f); }
}

void PrintChat(const char* message) {
    JNIEnv* env = GetEnv();
    if (!env) { DebugLog("[PrintChat] env null"); return; }

    jobject mcObj = GetMinecraftObject(env);
    if (!mcObj) { DebugLog("[PrintChat] mc null"); return; }

    // Get thePlayer: confirmed field "h" type "bew" in Badlion 1.8.9
    jclass mcCls = env->FindClass("ave");
    if (!mcCls) { ClearException(env); DebugLog("[PrintChat] ave not found"); return; }
    jfieldID pFid = env->GetFieldID(mcCls, "h", "Lbew;");
    ClearException(env);
    if (!pFid) { DebugLog("[PrintChat] thePlayer field (h/bew) not found"); return; }
    jobject player = env->GetObjectField(mcObj, pFid);
    if (!player) { DebugLog("[PrintChat] thePlayer is null"); return; }

    // Try to instantiate ChatComponentText — obfuscated name varies per build.
    // We try the most common 1.8.9 candidates and log which one succeeds.
    struct { const char* cls; const char* sig; } chatCandidates[] = {
        { "ir",  "(Ljava/lang/String;)V" },
        { "iq",  "(Ljava/lang/String;)V" },
        { "iy",  "(Ljava/lang/String;)V" },
        { "iz",  "(Ljava/lang/String;)V" },
        { "is",  "(Ljava/lang/String;)V" },
        { nullptr, nullptr }
    };
    jclass    chatCls  = nullptr;
    jmethodID chatInit = nullptr;
    for (int i = 0; chatCandidates[i].cls; i++) {
        chatCls = env->FindClass(chatCandidates[i].cls);
        ClearException(env);
        if (!chatCls) continue;
        chatInit = env->GetMethodID(chatCls, "<init>", chatCandidates[i].sig);
        ClearException(env);
        if (chatInit) {
            char buf[128]; snprintf(buf, sizeof(buf), "[PrintChat] ChatComponentText = %s", chatCandidates[i].cls);
            DebugLog(buf); break;
        }
        chatCls = nullptr;
    }
    if (!chatCls || !chatInit) { DebugLog("[PrintChat] ChatComponentText class not found — check log"); return; }

    jstring  jmsg     = env->NewStringUTF(message);
    jobject  chatComp = env->NewObject(chatCls, chatInit, jmsg);
    env->DeleteLocalRef(jmsg);
    if (!chatComp) { ClearException(env); DebugLog("[PrintChat] NewObject failed"); return; }

    // Call EntityPlayer.addChatMessage(IChatComponent).
    // Obfuscated method name is "a" in 1.8.9; try several IChatComponent type sigs.
    jclass playerCls = env->GetObjectClass(player);
    struct { const char* sig; } msgSigs[] = {
        { "(Lbms;)V" }, { "(Lbmq;)V" }, { "(Lbmo;)V" },
        { "(Lbmt;)V" }, { "(Lbmr;)V" }, { nullptr }
    };
    jmethodID addChat = nullptr;
    for (int i = 0; msgSigs[i].sig; i++) {
        addChat = env->GetMethodID(playerCls, "a", msgSigs[i].sig);
        ClearException(env);
        if (addChat) {
            char buf[128]; snprintf(buf, sizeof(buf), "[PrintChat] addChatMessage sig = %s", msgSigs[i].sig);
            DebugLog(buf); break;
        }
    }

    if (addChat) {
        env->CallVoidMethod(player, addChat, chatComp);
        ClearException(env);
        DebugLog("[PrintChat] message sent OK");
    } else {
        DebugLog("[PrintChat] addChatMessage method not found");
    }

    env->DeleteLocalRef(chatComp);
    env->DeleteLocalRef(player);
}

} // namespace SDK
