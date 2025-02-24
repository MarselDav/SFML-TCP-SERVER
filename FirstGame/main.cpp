#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <map>
#include <vector>

const unsigned short PORT = 5000;
const unsigned SHIPS_CNT = 10;
const std::wstring DEFAULT_NAME = L"Без имени";

const std::wstring SEND_PACKET_ERROR = L"Ошибка отправки пакета";
const std::wstring SEND_PACKET_SUCCESSFUL = L"Пакет успешно отправлен";

//const sf::IpAddress ipAddress = "0.0.0.0";

sf::TcpSocket* waitingPlayer = nullptr;

const enum Commands
{
    setPlayerName = 0,
    preparationMode = 1,
    waitingMode = 2,
    readyToBattle = 3,
    battleMode = 4,
    yourMove = 5,
    opponentMove = 6,
    shot = 7,
    yourHit = 8,
    yourMiss = 9,
    opponentHit = 10,
    opponentMiss = 11,
    destroyed = 12,
    youWin = 13,
    youLose = 14,
};

struct PlayersInfo
{
    std::map<sf::TcpSocket*, std::wstring> clients_map; // словарь для хранения сокетов клиентов и их имён
    std::vector<sf::TcpSocket*> clientsToDelete_vector; // вектор клиентов, подлежащих удалению


    std::map<sf::TcpSocket*, sf::TcpSocket*> opponents_map; // словарь оппонентов

    std::map<sf::TcpSocket*, bool> playersIsTurn_map; // словарь ходит сейчас игрок или нет
    std::map<sf::TcpSocket*, std::vector<std::vector<int>>> playersTables_map; // словарь расставленных кораблей игроков
    std::map<sf::TcpSocket*, std::vector<std::vector<int>>> playersShots_map; // словарь со списками выстрелов игрока
    std::map<sf::TcpSocket*, std::vector<std::vector<int>>> playerHits_map; // словарь со списками попаданий игрока
    std::map<sf::TcpSocket*, int> playerDestroyedCount_map; // словарь со списками попаданий игрока

    void disconnectingClient(sf::TcpSocket* socketToDelete)
    {
        if (waitingPlayer == socketToDelete)
            waitingPlayer = nullptr;

        if (clients_map.find(socketToDelete) != clients_map.end()) {
            clients_map.erase(socketToDelete);
        }

        if (opponents_map.find(socketToDelete) != opponents_map.end()) {
            opponents_map.erase(socketToDelete);
        }

        if (playersIsTurn_map.find(socketToDelete) != playersIsTurn_map.end()) {
            playersIsTurn_map.erase(socketToDelete);
        }

        if (playersTables_map.find(socketToDelete) != playersTables_map.end()) {
            playersTables_map.erase(socketToDelete);
        }

        if (playersShots_map.find(socketToDelete) != playersShots_map.end()) {
            playersShots_map.erase(socketToDelete);
        }

        if (playerHits_map.find(socketToDelete) != playerHits_map.end()) {
            playerHits_map.erase(socketToDelete);
        }

        if (playerDestroyedCount_map.find(socketToDelete) != playerDestroyedCount_map.end()) {
            playerDestroyedCount_map.erase(socketToDelete);
        }
    }
};


// Диапазон чисел: [min, max]
int GetRandomNumber(int min, int max)
{
    // Установить генератор случайных чисел
    srand(time(NULL));

    // Получить случайное число - формула
    int num = min + rand() % (max - min + 1);

    return num;
}

bool isMoveRepeat(sf::TcpSocket* client, PlayersInfo& playersinfo, int x, int y)
{
    std::vector<std::vector<int>> playerShots_vector = playersinfo.playersShots_map[client];

    bool isRepeat = false;
    int i = 0;
    while (i < playerShots_vector.size() && !isRepeat)
    {
        if (playerShots_vector[i][0] == x && playerShots_vector[i][1] == y)
        {
            isRepeat = true;
        }
        i++;
    }
    return isRepeat;
}


sf::Socket::Status sendPacket(sf::TcpSocket& client, sf::Packet packet)
{
    sf::Socket::Status status = client.send(packet);
    if (status != sf::Socket::Done)
    {
        std::wcout << SEND_PACKET_ERROR << std::endl;
    }
    else
    {
        //std::wcout << SEND_PACKET_SUCCESSFUL << std::endl;
    }

    return status;
}


template <typename T>
sf::Socket::Status sendToPlayer(sf::TcpSocket& client, int command, int size, const T* dataArray)
{
    sf::Packet packet;
    packet << command;
    for (int i = 0; i < size; i++)
    {
        packet << dataArray[i];
    }

    return sendPacket(client, packet);
}

sf::Socket::Status sendToPlayer(sf::TcpSocket& client, int command)
{
    sf::Packet packet;
    packet << command;

    return sendPacket(client, packet);
}


bool connectWithAnotherPlayer(sf::TcpSocket* client, PlayersInfo & playersinfo, sf::TcpSocket * waitingPlayer)
{
    std::wstring player2NameString[] = { playersinfo.clients_map[client] };
    sf::Socket::Status player1Status = sendToPlayer(*waitingPlayer, preparationMode, 1, player2NameString);

    std::wstring player1NameString[] = { playersinfo.clients_map[waitingPlayer] };
    sf::Socket::Status player2Status = sendToPlayer(*client, preparationMode, 1, player1NameString);

    bool isConnectionSuccesful = false;

    if (player1Status == sf::Socket::Done && player2Status == sf::Socket::Done)
    {
        playersinfo.opponents_map[client] = waitingPlayer;
        playersinfo.opponents_map[waitingPlayer] = client;
        waitingPlayer = nullptr;
        isConnectionSuccesful = true;
    }
    else
    {
        if (player1Status != sf::Socket::Done)
        {
            waitingPlayer = nullptr;
        }

        if (player2Status == sf::Socket::Done)
        {
            waitingPlayer = client;
        }
    }
    return isConnectionSuccesful;
}

void readyToBattleCommandProcessing(sf::TcpSocket *client, sf::Packet packet, PlayersInfo & playersinfo)
{
    if (playersinfo.playersTables_map.find(client) == playersinfo.playersTables_map.end())
    {
        std::vector< std::vector<int>> table;
        for (int i = 0; i < SHIPS_CNT; i++)
        {
            int x, y, w, h;
            packet >> x;
            packet >> y;
            packet >> w;
            packet >> h;
            std::vector<int> shipsParam = { x, y, w, h };
            table.push_back(shipsParam);
        }
        playersinfo.playersTables_map[client] = table;


        if (playersinfo.opponents_map.find(client) != playersinfo.opponents_map.end() && table.size() == SHIPS_CNT) // если противник существует
        {
            sf::TcpSocket* opponent = playersinfo.opponents_map.find(client)->second;
            if (playersinfo.playersTables_map.find(opponent) != playersinfo.playersTables_map.end()) // если противник уже расставил корабли
            {
                sendToPlayer(*client, Commands::battleMode); // отправляем игроку команду начала битвы
                playersinfo.playerDestroyedCount_map[client] = 0;
                sendToPlayer(*opponent, Commands::battleMode); // отправляем оппоненту команду начала битвы
                playersinfo.playerDestroyedCount_map[opponent] = 0;

                int whose_move = GetRandomNumber(0, 1);
                if (whose_move == 0)
                {
                    sendToPlayer(*client, Commands::yourMove); // отправляем игроку команду, что его ход
                    playersinfo.playersIsTurn_map[client] = true;
                    sendToPlayer(*opponent, Commands::opponentMove); // отправляем игроку команду, что сейчас ход противника
                    playersinfo.playersIsTurn_map[opponent] = false;
                }
                else
                {
                    sendToPlayer(*client, Commands::opponentMove); // отправляем игроку команду начала битвы
                    playersinfo.playersIsTurn_map[client] = false;
                    sendToPlayer(*opponent, Commands::yourMove); // отправляем игроку команду начала битвы
                    playersinfo.playersIsTurn_map[opponent] = true;
                }

            }
            else
                sendToPlayer(*client, Commands::waitingMode);
        }
        else
        {
            std::cout << "Произошла ошибка" << std::endl;
        }
    }
    else
    {
        std::cout << "Произошла ошибка, корабли уже расставлены" << std::endl;
    }
}

void shotCommandProcessing(sf::TcpSocket* client, sf::Packet packet, PlayersInfo& playersinfo)
{
    if (playersinfo.playersIsTurn_map[client])
    {
        int cells_count_not_destroyed;
        int c_x, c_y;
        packet >> c_x;
        packet >> c_y;
        if (!isMoveRepeat(client, playersinfo, c_x, c_y)) // проверка ходил ли уже игрок в эту клетку
        {
            playersinfo.playersShots_map[client].push_back({ c_x , c_y });

            int hitCordsInCell[] = { c_x, c_y };
            int arraySize = sizeof(hitCordsInCell) / sizeof(int);

            sf::TcpSocket* opponent = playersinfo.opponents_map.find(client)->second;

            std::vector<std::vector<int>> ships = playersinfo.playersTables_map[opponent];
            bool isHitTheShip = false;
            int i = 0;
            while (i < ships.size() && !isHitTheShip)
            {
                sf::IntRect shipRect = sf::IntRect(ships[i][0], ships[i][1], ships[i][2], ships[i][3]); // прямоугольник рассматриваемого корабля
                //std::cout << "прямоугольник рассматриваемого корабля: " << ships[i][0] << " " << ships[i][1] << " " << ships[i][2] << " " << ships[i][3] << std::endl;
                sf::IntRect shotRect = sf::IntRect(c_x, c_y, 1, 1); // прямоугольник выстрела
                //std::cout << "прямоугольник выстрела: " << c_x << " " << c_y << " " << 1<< " " << 1 << std::endl;
                if (shipRect.intersects(shotRect)) // проверка попал ли в корабль
                {
                    isHitTheShip = true;
                    std::vector<std::vector<int>> playerHits = playersinfo.playerHits_map[client]; // список попаданий игрока
                    // проверяем были ли другие попадания в этот корабль
                    cells_count_not_destroyed = (ships[i][2] > ships[i][3] ? ships[i][2] : ships[i][3]) - 1; // количество не уничтоженных клеток

                    int j = 0;
                    while (j != playerHits.size() && cells_count_not_destroyed != 0)
                    {
                        sf::IntRect hitRect = sf::IntRect(playerHits[j][0], playerHits[j][1], 1, 1); // прямоугольник попадания
                        if (shipRect.intersects(hitRect)) // проверка попал ли именно в этот корабль
                        {
                            cells_count_not_destroyed--; // уменьшаем количество не уничтоженных клеток
                        }
                        j++;
                    }
                    if (cells_count_not_destroyed == 0) // если клеток не осталось, то корабль уничтожен
                    {
                        std::cout << "Количество уничтоженных кораблей до: " << playersinfo.playerDestroyedCount_map[client] << std::endl;
                        playersinfo.playerDestroyedCount_map[client]++; // увеличиваем кол-во уничтоженных игроком кораблей
                        std::cout << "Количество уничтоженных кораблей после: " << playersinfo.playerDestroyedCount_map[client] << std::endl;
                        int shipCordsInCell[] = { ships[i][0], ships[i][1], ships[i][2], ships[i][3] };
                        sendToPlayer(*client, Commands::destroyed, 4, shipCordsInCell); // команда что корабль уничтожен ships[i][0], ships[i][1]
                    }

                    playerHits.push_back({ c_x , c_y }); // добавление нового попадания в список
                    playersinfo.playerHits_map[client] = playerHits; // обновления списка в словаре
                }
                i++;
            }

            if (isHitTheShip)
            {
                std::cout << "попадание: " << c_x << " " << c_y << std::endl;
                sendToPlayer(*client, Commands::yourHit, arraySize, hitCordsInCell);
                sendToPlayer(*opponent, Commands::opponentHit, arraySize, hitCordsInCell);
            }
            else
            {
                std::cout << "промах: " << c_x << " " << c_y << std::endl;
                sendToPlayer(*client, Commands::yourMiss, arraySize, hitCordsInCell);
                sendToPlayer(*opponent, Commands::opponentMiss, arraySize, hitCordsInCell);
            }

            if (playersinfo.playerDestroyedCount_map[client] == SHIPS_CNT)
            {
                sendToPlayer(*client, Commands::youWin);
                sendToPlayer(*opponent, Commands::youLose);
            }
            else
            {
                // ход переходит оппоненту
                sendToPlayer(*opponent, Commands::yourMove);
                playersinfo.playersIsTurn_map[opponent] = true;
                sendToPlayer(*client, Commands::opponentMove);
                playersinfo.playersIsTurn_map[client] = false;
            }

        }
        else
        {
            std::cout << "Игрок уже ходил в эту клетку" << std::endl;
        }
    }
    else
    {
        std::cout << "Сейчас не ход этого игрока" << std::endl;
    }
    
}

void dataProcessing(sf::TcpSocket* client, sf::Packet packet, PlayersInfo & playersinfo)
{
    int command;
    packet >> command;
    switch (command)
    {
    case (Commands::setPlayerName):
    {
        std::wstring name;
        packet >> name;
        //std::cout << name << std::endl;
        playersinfo.clients_map[client] = name;
        if (waitingPlayer != nullptr)
        {
            //std::cout << "в чем проблема бляяяяяять" << std::endl;
            bool isConnectionSuccesful = connectWithAnotherPlayer(client, playersinfo, waitingPlayer);
            if (!isConnectionSuccesful)
            {
                sendToPlayer(*client, Commands::preparationMode);
            }
        }
        else
        {
            waitingPlayer = client;
            sendToPlayer(*client, Commands::waitingMode);
        }
        break;
    }
    case (Commands::readyToBattle):
    {
        readyToBattleCommandProcessing(client, packet, playersinfo);
        break;
    }
    case(Commands::shot):
    {
        shotCommandProcessing(client, packet, playersinfo);
        break;
    }
    default:
        break;
    }
}


// возможно не нужна
bool checkPlayerHaveOpponent(sf::TcpSocket* client, std::map<sf::TcpSocket*, sf::TcpSocket*> &opponents_map)
{
    bool isFree = false;
    std::map<sf::TcpSocket*, sf::TcpSocket*>::iterator opponentsIt;
    opponentsIt = opponents_map.begin();
    while (opponentsIt != opponents_map.end() && isFree)
    {
        sf::TcpSocket* opponent1 = opponentsIt->first;
        sf::TcpSocket* opponent2 = opponentsIt->second;
        if (client == opponent1 || client == opponent2)
        {
            isFree = false;
        }
        opponentsIt++;
    }
    return isFree;
}


int main()
{
    setlocale(LC_ALL, "rus");

    PlayersInfo playersinfo;

    // Создаем селектор для мультиплексирования
    sf::SocketSelector selector;
    //sf::TcpSocket* waitingPlayer = nullptr;
    // Создаем слушающий сокет на порту 5000
    sf::IpAddress ipAddress;
    std::cout << "Введи ip адрес" << std::endl;
    std::cin >> ipAddress;
    



    sf::TcpListener listener;
    if (listener.listen(PORT, ipAddress) != sf::Socket::Done)
    {
        std::cout << "Ошибка при запуске сервера на порту " << PORT << std::endl;
        return 1;
    }

    std::cout << "Сервер запущен на порту " << PORT << std::endl;

    selector.add(listener);

    // Главный цикл сервера
    bool serverIsOpen = true;
    while (serverIsOpen)
    {
        // Ожидаем события ввода-вывода
        if (selector.wait())
        {
            // Проверяем, есть ли новое входящее соединение
            if (selector.isReady(listener))
            {
                sf::TcpSocket* client = new sf::TcpSocket;
                if (listener.accept(*client) == sf::Socket::Done)
                {
                    // Добавляем клиента в список
                    client->setBlocking(false);
                    playersinfo.clients_map.insert(std::pair<sf::TcpSocket*, std::wstring>(client, DEFAULT_NAME));
                    std::cout << "Новый клиент подключен (" << playersinfo.clients_map.size() << " всего)" << std::endl;

                    // Добавляем сокет клиента в селектор
                    selector.add(*client);
                }
                else
                {
                    std::cout << "Ошибка подключения клиента" << std::endl;
                    delete client;
                }
            }
            else
            {
                // Обрабатываем события для каждого клиента
                for (auto it = playersinfo.clients_map.begin(); it != playersinfo.clients_map.end();)
                {
                    sf::TcpSocket* client = it->first;

                    if (selector.isReady(*client))
                    {
                        sf::Packet packet;
                        if (client->receive(packet) == sf::Socket::Done)
                        {
                            // Данные получены успешно, обрабатываем их
                            // Обработка данных...
                            dataProcessing(client, packet, playersinfo);
                            ++it;
                        }
                        else
                        {
                            // Ошибка при получении данных от клиента, отключаем его
                            playersinfo.clientsToDelete_vector.push_back(client);
                            selector.remove(*client);
                            client->disconnect();
                            delete client;
                        }
                    }
                    else
                    {
                        ++it;
                    }
                }
                for (int i = 0; i < playersinfo.clientsToDelete_vector.size(); i++)
                {
                    std::wcout << L"Клиент: " << playersinfo.clients_map[playersinfo.clientsToDelete_vector[i]] << L" отключен" << std::endl;
                    playersinfo.disconnectingClient(playersinfo.clientsToDelete_vector[i]);
                    //delete playersinfo.clientsToDelete_vector[i];
                }
                playersinfo.clientsToDelete_vector.clear();
            }
        }
    }

    // Закрываем соединение с клиентами
    std::map<sf::TcpSocket*, std::wstring>::iterator clientsIt;
    for (clientsIt = playersinfo.clients_map.begin(); clientsIt != playersinfo.clients_map.end(); clientsIt++)
    {
        sf::TcpSocket* client = clientsIt->first;
        client->disconnect();
        delete client;
    }
    return 0;
}