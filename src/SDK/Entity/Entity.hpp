#pragma once
#include <jni.h>
#include <string>

// Wrapper around Entity / EntityLivingBase / EntityPlayer hierarchy.
// 1.8.9 obfuscated: Entity = "ps", EntityLivingBase = "sv", EntityPlayer = "bew"
class Entity {
public:
    Entity(JNIEnv* env, jobject obj) : m_env(env), m_obj(obj) {}

    bool    isValid() const { return m_obj != nullptr; }
    jobject getObject() const { return m_obj; }

    double getPosX() const;
    double getPosY() const;
    double getPosZ() const;

    float  getYaw()   const;
    float  getPitch() const;
    void   setYaw(float yaw);
    void   setPitch(float pitch);

    float  getHealth() const;
    bool   isAlive()   const;
    bool   isPlayer()  const;

    double getDistanceTo(const Entity& other) const;
    std::string getName() const;

private:
    double getDoubleField(const char* cls, const char* name) const;
    float  getFloatField(const char* cls, const char* name)  const;
    void   setFloatField(const char* cls, const char* name, float v);

    JNIEnv* m_env;
    jobject m_obj;
};
