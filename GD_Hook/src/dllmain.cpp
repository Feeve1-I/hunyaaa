#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include "MinHook.h"

// Определение указателя на функцию CCScheduler::update
// В 32-битном Cocos2d-x (используется в GD) методы классов имеют конвенцию __thiscall
// CCScheduler::update принимает 1 аргумент (помимо скрытого this) — float dt
typedef void(__thiscall* CCScheduler_update)(void* self, float dt);

CCScheduler_update fpOriginalUpdate = nullptr;

// Наш кастомный DeltaTime (здесь мы ставим 10 FPS, чтобы солвер успевал считать)
// По умолчанию в игре 1.0f / 60.0f или 1.0f / 240.0f
float CUSTOM_DELTA_TIME = 1.0f / 10.0f; 

// Наша функция-перехватчик
void __thiscall DetourUpdate(void* self, float dt) {
    // Вместо реального времени (dt), которое прошло между кадрами, 
    // мы подсовываем игре наш фиксированный шаг. 
    // Это создает "покадровый" режим для идеальной симуляции.
    fpOriginalUpdate(self, CUSTOM_DELTA_TIME);
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    // Открываем консоль для дебага DLL
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    std::cout << "[+] Speedhack DLL успешно заинжекчена!" << std::endl;

    // Инициализация библиотеки MinHook
    if (MH_Initialize() != MH_OK) {
        std::cout << "[-] Ошибка инициализации MinHook." << std::endl;
        return 1;
    }

    // Получаем базовый адрес libcocos2d.dll
    HMODULE hCocos = GetModuleHandleA("libcocos2d.dll");
    if (!hCocos) {
        std::cout << "[-] libcocos2d.dll не найдена. Игра не запущена или другая архитектура." << std::endl;
        return 1;
    }

    // Ищем функцию CCScheduler::update по её экспортируемому имени (mangled name).
    // Это имя актуально для большинства сборок cocos2d.
    void* updateFuncAddr = GetProcAddress(hCocos, "?update@CCScheduler@cocos2d@@QAEXM@Z");
    
    if (!updateFuncAddr) {
        std::cout << "[-] Не удалось найти CCScheduler::update через GetProcAddress." << std::endl;
        // ЗАГЛУШКА: Если имя функции было изменено, сюда нужно вписать смещение (Offset)
        // Примерно так: updateFuncAddr = (void*)((DWORD)hCocos + 0xABCDEF);
        std::cout << "[-] Требуется ручное обновление адреса (PLACEHOLDER)." << std::endl;
    } else {
        std::cout << "[+] Функция CCScheduler::update найдена: 0x" << std::hex << updateFuncAddr << std::endl;
    }

    // Создаем хук
    if (MH_CreateHook(updateFuncAddr, &DetourUpdate, reinterpret_cast<LPVOID*>(&fpOriginalUpdate)) != MH_OK) {
        std::cout << "[-] Ошибка создания хука." << std::endl;
        return 1;
    }

    // Включаем хук
    if (MH_EnableHook(updateFuncAddr) != MH_OK) {
        std::cout << "[-] Ошибка активации хука." << std::endl;
        return 1;
    }

    std::cout << "[+] Speedhack активирован!" << std::endl;
    std::cout << "[+] DeltaTime зафиксирован на: " << CUSTOM_DELTA_TIME << " (10 кадрах в секунду)" << std::endl;
    std::cout << "[*] Чтобы выгрузить DLL, закройте это окно консоли." << std::endl;

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        // Убираем за собой при выгрузке
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        FreeConsole();
        break;
    }
    return TRUE;
}
