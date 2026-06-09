import pymem
import pymem.process
import pymem.exception
import time
import sys

# =====================================================================
# ЗАГЛУШКИ ОФСЕТОВ (PLACEHOLDER OFFSETS) ДЛЯ GD 2.2+
# Впишите сюда реальные офсеты, когда найдете их через Cheat Engine 
# или исходники Geode SDK
# =====================================================================

# 1. Базовый адрес (например, GameManager или BaseApp)
GAME_MANAGER_OFFSET = 0x000000 

# 2. Цепочка смещений (Pointer Chain) от GameManager до PlayerObject (Player1)
PLAYER_CHAIN = [0x00, 0x00] 

# 3. Смещения конкретных параметров внутри структуры PlayerObject
OFFSET_X = 0x00          # Тип: Float (Координата X)
OFFSET_Y = 0x00          # Тип: Float (Координата Y)
OFFSET_IS_DEAD = 0x00    # Тип: Byte или Bool (0 - жив, 1 - мертв)

# =====================================================================

def get_pointer_address(pm: pymem.Pymem, base_addr: int, offsets: list) -> int:
    """
    Проходит по цепочке указателей (для 32-битной архитектуры GD).
    """
    try:
        addr = pm.read_uint(base_addr)
        for offset in offsets[:-1]:
            if addr == 0:
                return 0
            addr = pm.read_uint(addr + offset)
        return addr + offsets[-1] if addr != 0 else 0
    except pymem.exception.MemoryReadError:
        return 0

def main():
    print("[*] Ожидание запуска GeometryDash.exe...")
    
    # Пытаемся подключиться к процессу
    pm = None
    while not pm:
        try:
            pm = pymem.Pymem("GeometryDash.exe")
            print("[+] Успешно подключено к GeometryDash.exe!")
        except pymem.exception.ProcessNotFound:
            time.sleep(1)

    # Получаем базовый адрес модуля GeometryDash.exe
    module = pymem.process.module_from_name(pm.process_handle, "GeometryDash.exe")
    base_address = module.lpBaseOfDll
    
    print(f"[+] Базовый адрес GeometryDash.exe: {hex(base_address)}")
    print("[*] Запуск цикла чтения памяти (нажмите Ctrl+C для выхода)\n")

    try:
        while True:
            # Вычисляем актуальный адрес игрока в памяти
            game_manager_addr = base_address + GAME_MANAGER_OFFSET
            player_addr = get_pointer_address(pm, game_manager_addr, PLAYER_CHAIN)

            if player_addr != 0:
                try:
                    # Считываем данные куба
                    player_x = pm.read_float(player_addr + OFFSET_X)
                    player_y = pm.read_float(player_addr + OFFSET_Y)
                    # Обычно isDead хранится как 1 байт (boolean)
                    is_dead_val = pm.read_bytes(player_addr + OFFSET_IS_DEAD, 1)
                    is_dead = int.from_bytes(is_dead_val, byteorder='little') != 0

                    # Форматированный вывод
                    status = "МЕРТВ 💀" if is_dead else "ЖИВ 🟢"
                    sys.stdout.write(f"\r[Player] X: {player_x:8.2f} | Y: {player_y:8.2f} | Статус: {status}    ")
                    sys.stdout.flush()

                except pymem.exception.MemoryReadError:
                    sys.stdout.write("\r[!] Ошибка чтения памяти. Возможно, мы не в уровне...    ")
                    sys.stdout.flush()
            else:
                sys.stdout.write("\r[!] Указатель на игрока не найден (PlayLayer не активен?)    ")
                sys.stdout.flush()

            # Задержка 0.01s (около 100 Г��)
            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n\n[*] Остановка бота...")
    except Exception as e:
        print(f"\n[!] Произошла ошибка: {e}")
    finally:
        pm.close_process()

if __name__ == "__main__":
    main()
