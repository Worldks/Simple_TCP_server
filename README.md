# Overview  
The server maintains a counter that can be modified by any connected clients. The server accepts text strings from
clients, parses them, and executes them if the string contains one of three commands (up, down, show).
Clients can use Telnet to interact with the server. The server performs I/O multiplexing.
The server operates correctly with a large number of clients connecting and disconnecting.
The only limitation on the number of clients served may be the maximum number of descriptors
that can be simultaneously open in a process.  

Сервер хранит счетчик который могут изменить любые подключенные клиенты. Сервер принимает текстовые строки от   
клиента, анализирует их если в строке есть одна из трёх команд (up, down, show) выполняет её.  
Клиенты могут использовать telnet для взаимодействия с сервером. Серевер производит мультиплексирование ввода-вывода.  
Сервер корректно работает при большом большом количестве подключаемых и отключаемых клиентов.  
Единственное ограничение на количество обслуживаемых клиентов может быть обусловлено предельным числом  
одновременно открытых в процессе дескрипторов.  


# Build  
>gcc -Wall -g main.c -o server
