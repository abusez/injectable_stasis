#pragma once
#include "../../Module.hpp"
#include <chrono>

class Stasis : public Module {
public:
    Stasis();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
    void onRender()  override;

private:
    jclass    m_entityClass  = nullptr;
    jclass    m_playerClass  = nullptr;
    jclass    m_livingClass  = nullptr;

    jfieldID  m_motionX      = nullptr;
    jfieldID  m_motionY      = nullptr;
    jfieldID  m_motionZ      = nullptr;

    jfieldID  m_hurtTime     = nullptr;
    jfieldID  m_moveInput    = nullptr;
    jfieldID  m_moveForward  = nullptr;
    jfieldID  m_moveStrafe   = nullptr;
    jfieldID  m_lastRptX     = nullptr;
    jfieldID  m_lastRptY     = nullptr;
    jfieldID  m_lastRptZ     = nullptr;

    bool   m_cached    = false;

    // Pulse — briefly suspends stasis every N ticks to send a real position
    // packet, preventing anti-cheat timeout/void-prediction flags.
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_lastPulse;
    Clock::time_point m_pulseEnd;
    bool              m_pulsing  = false;

    void cacheIDs(JNIEnv* env);
};
