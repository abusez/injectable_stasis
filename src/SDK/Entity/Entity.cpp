#include "../../pch.hpp"
#include "Entity.hpp"
#include "../../SDK/sdk.hpp"

double Entity::getDoubleField(const char* cls, const char* name) const {
    if (!m_obj) return 0.0;
    jclass c = m_env->FindClass(cls);
    if (!c) { SDK::ClearException(m_env); return 0.0; }
    jfieldID fid = m_env->GetFieldID(c, name, "D");
    if (!fid) { SDK::ClearException(m_env); return 0.0; }
    return m_env->GetDoubleField(m_obj, fid);
}

float Entity::getFloatField(const char* cls, const char* name) const {
    if (!m_obj) return 0.0f;
    jclass c = m_env->FindClass(cls);
    if (!c) { SDK::ClearException(m_env); return 0.0f; }
    jfieldID fid = m_env->GetFieldID(c, name, "F");
    if (!fid) { SDK::ClearException(m_env); return 0.0f; }
    return m_env->GetFloatField(m_obj, fid);
}

void Entity::setFloatField(const char* cls, const char* name, float v) {
    if (!m_obj) return;
    jclass c = m_env->FindClass(cls);
    if (!c) { SDK::ClearException(m_env); return; }
    jfieldID fid = m_env->GetFieldID(c, name, "F");
    if (!fid) { SDK::ClearException(m_env); return; }
    m_env->SetFloatField(m_obj, fid, v);
}

// Minecraft 1.8.9 Entity (ps) field names:
//   posX="s" posY="t" posZ="u"  rotationYaw="y"  rotationPitch="z"

double Entity::getPosX() const { return getDoubleField("ps", "s"); }
double Entity::getPosY() const { return getDoubleField("ps", "t"); }
double Entity::getPosZ() const { return getDoubleField("ps", "u"); }
float  Entity::getYaw()   const { return getFloatField("ps", "y"); }
float  Entity::getPitch() const { return getFloatField("ps", "z"); }
void   Entity::setYaw(float v)   { setFloatField("ps", "y", v); }
void   Entity::setPitch(float v) { setFloatField("ps", "z", v); }

float Entity::getHealth() const {
    if (!m_obj) return 0.0f;
    jclass c = m_env->FindClass("sv"); // EntityLivingBase in 1.8.9
    if (!c) { SDK::ClearException(m_env); return 0.0f; }
    jmethodID mid = m_env->GetMethodID(c, "bm", "()F"); // getHealth()
    if (!mid) { SDK::ClearException(m_env); return 0.0f; }
    return m_env->CallFloatMethod(m_obj, mid);
}

bool Entity::isAlive() const { return getHealth() > 0.0f; }

bool Entity::isPlayer() const {
    if (!m_obj) return false;
    jclass pc = m_env->FindClass("bew"); // EntityPlayer in 1.8.9
    if (!pc) { SDK::ClearException(m_env); return false; }
    return m_env->IsInstanceOf(m_obj, pc) == JNI_TRUE;
}

double Entity::getDistanceTo(const Entity& other) const {
    double dx = getPosX() - other.getPosX();
    double dy = getPosY() - other.getPosY();
    double dz = getPosZ() - other.getPosZ();
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

std::string Entity::getName() const {
    if (!m_obj) return "";
    jclass c = m_env->FindClass("ps");
    if (!c) { SDK::ClearException(m_env); return ""; }
    // getName() — "al" in Entity (ps) 1.8.9
    jmethodID mid = m_env->GetMethodID(c, "al", "()Ljava/lang/String;");
    if (!mid) { SDK::ClearException(m_env); return ""; }
    auto* s = (jstring)m_env->CallObjectMethod(m_obj, mid);
    return s ? SDK::JStringToStr(m_env, s) : "";
}
