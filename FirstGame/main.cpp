#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <map>
#include <vector>

const unsigned short PORT = 5000;
const unsigned SHIPS_CNT = 10;
const std::wstring DEFAULT_NAME = L"��� �����";

const std::wstring SEND_PACKET_ERROR = L"������ �������� ������";
const std::wstring SEND_PACKET_SUCCESSFUL = L"����� ������� ���������";

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
    std::map<sf::TcpSocket*, std::wstring> clients_map; // ������� ��� �������� ������� �������� � �� ���
    std::vector<sf::TcpSocket*> clientsToDelete_vector; // ������ ��������, ���������� ��������


    std::map<sf::TcpSocket*, sf::TcpSocket*> opponents_map; // ������� ����������

    std::map<sf::TcpSocket*, bool> playersIsTurn_map; // ������� ����� ������ ����� ��� ���
    std::map<sf::TcpSocket*, std::vector<std::vector<int>>> playersTables_map; // ������� ������������� �������� �������
    std::map<sf::TcpSocket*, std::vector<std::vector<int>>> playersShots_map; // ������� �� �������� ��������� ������
    std::map<sf::TcpSocket*, std::vector<std::vector<int>>> playerHits_map; // ������� �� �������� ��������� ������
    std::map<sf::TcpSocket*, int> playerDestroyedCount_map; // ������� �� �������� ��������� ������

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


// �������� �����: [min, max]
int GetRandomNumber(int min, int max)
{
    // ���������� ��������� ��������� �����
    srand(time(NULL));

    // �������� ��������� ����� - �������
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


        if (playersinfo.opponents_map.find(client) != playersinfo.opponents_map.end() && table.size() == SHIPS_CNT) // ���� ��������� ����������
        {
            sf::TcpSocket* opponent = playersinfo.opponents_map.find(client)->second;
            if (playersinfo.playersTables_map.find(opponent) != playersinfo.playersTables_map.end()) // ���� ��������� ��� ��������� �������
            {
                sendToPlayer(*client, Commands::battleMode); // ���������� ������ ������� ������ �����
                playersinfo.playerDestroyedCount_map[client] = 0;
                sendToPlayer(*opponent, Commands::battleMode); // ���������� ��������� ������� ������ �����
                playersinfo.playerDestroyedCount_map[opponent] = 0;

                int whose_move = GetRandomNumber(0, 1);
                if (whose_move == 0)
                {
                    sendToPlayer(*client, Commands::yourMove); // ���������� ������ �������, ��� ��� ���
                    playersinfo.playersIsTurn_map[client] = true;
                    sendToPlayer(*opponent, Commands::opponentMove); // ���������� ������ �������, ��� ������ ��� ����������
                    playersinfo.playersIsTurn_map[opponent] = false;
                }
                else
                {
                    sendToPlayer(*client, Commands::opponentMove); // ���������� ������ ������� ������ �����
                    playersinfo.playersIsTurn_map[client] = false;
                    sendToPlayer(*opponent, Commands::yourMove); // ���������� ������ ������� ������ �����
                    playersinfo.playersIsTurn_map[opponent] = true;
                }

            }
            else
                sendToPlayer(*client, Commands::waitingMode);
        }
        else
        {
            std::cout << "��������� ������" << std::endl;
        }
    }
    else
    {
        std::cout << "��������� ������, ������� ��� �����������" << std::endl;
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
        if (!isMoveRepeat(client, playersinfo, c_x, c_y)) // �������� ����� �� ��� ����� � ��� ������
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
                sf::IntRect shipRect = sf::IntRect(ships[i][0], ships[i][1], ships[i][2], ships[i][3]); // ������������� ���������������� �������
                //std::cout << "������������� ���������������� �������: " << ships[i][0] << " " << ships[i][1] << " " << ships[i][2] << " " << ships[i][3] << std::endl;
                sf::IntRect shotRect = sf::IntRect(c_x, c_y, 1, 1); // ������������� ��������
                //std::cout << "������������� ��������: " << c_x << " " << c_y << " " << 1<< " " << 1 << std::endl;
                if (shipRect.intersects(shotRect)) // �������� ����� �� � �������
                {
                    isHitTheShip = true;
                    std::vector<std::vector<int>> playerHits = playersinfo.playerHits_map[client]; // ������ ��������� ������
                    // ��������� ���� �� ������ ��������� � ���� �������
                    cells_count_not_destroyed = (ships[i][2] > ships[i][3] ? ships[i][2] : ships[i][3]) - 1; // ���������� �� ������������ ������

                    int j = 0;
                    while (j != playerHits.size() && cells_count_not_destroyed != 0)
                    {
                        sf::IntRect hitRect = sf::IntRect(playerHits[j][0], playerHits[j][1], 1, 1); // ������������� ���������
                        if (shipRect.intersects(hitRect)) // �������� ����� �� ������ � ���� �������
                        {
                            cells_count_not_destroyed--; // ��������� ���������� �� ������������ ������
                        }
                        j++;
                    }
                    if (cells_count_not_destroyed == 0) // ���� ������ �� ��������, �� ������� ���������
                    {
                        std::cout << "���������� ������������ �������� ��: " << playersinfo.playerDestroyedCount_map[client] << std::endl;
                        playersinfo.playerDestroyedCount_map[client]++; // ����������� ���-�� ������������ ������� ��������
                        std::cout << "���������� ������������ �������� �����: " << playersinfo.playerDestroyedCount_map[client] << std::endl;
                        int shipCordsInCell[] = { ships[i][0], ships[i][1], ships[i][2], ships[i][3] };
                        sendToPlayer(*client, Commands::destroyed, 4, shipCordsInCell); // ������� ��� ������� ��������� ships[i][0], ships[i][1]
                    }

                    playerHits.push_back({ c_x , c_y }); // ���������� ������ ��������� � ������
                    playersinfo.playerHits_map[client] = playerHits; // ���������� ������ � �������
                }
                i++;
            }

            if (isHitTheShip)
            {
                std::cout << "���������: " << c_x << " " << c_y << std::endl;
                sendToPlayer(*client, Commands::yourHit, arraySize, hitCordsInCell);
                sendToPlayer(*opponent, Commands::opponentHit, arraySize, hitCordsInCell);
            }
            else
            {
                std::cout << "������: " << c_x << " " << c_y << std::endl;
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
                // ��� ��������� ���������
                sendToPlayer(*opponent, Commands::yourMove);
                playersinfo.playersIsTurn_map[opponent] = true;
                sendToPlayer(*client, Commands::opponentMove);
                playersinfo.playersIsTurn_map[client] = false;
            }

        }
        else
        {
            std::cout << "����� ��� ����� � ��� ������" << std::endl;
        }
    }
    else
    {
        std::cout << "������ �� ��� ����� ������" << std::endl;
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
            //std::cout << "� ��� �������� ����������" << std::endl;
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


// �������� �� �����
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

    // ������� �������� ��� �������������������
    sf::SocketSelector selector;
    //sf::TcpSocket* waitingPlayer = nullptr;
    // ������� ��������� ����� �� ����� 5000
    sf::IpAddress ipAddress;
    std::cout << "����� ip �����" << std::endl;
    std::cin >> ipAddress;
    



    sf::TcpListener listener;
    if (listener.listen(PORT, ipAddress) != sf::Socket::Done)
    {
        std::cout << "������ ��� ������� ������� �� ����� " << PORT << std::endl;
        return 1;
    }

    std::cout << "������ ������� �� ����� " << PORT << std::endl;

    selector.add(listener);

    // ������� ���� �������
    bool serverIsOpen = true;
    while (serverIsOpen)
    {
        // ������� ������� �����-������
        if (selector.wait())
        {
            // ���������, ���� �� ����� �������� ����������
            if (selector.isReady(listener))
            {
                sf::TcpSocket* client = new sf::TcpSocket;
                if (listener.accept(*client) == sf::Socket::Done)
                {
                    // ��������� ������� � ������
                    client->setBlocking(false);
                    playersinfo.clients_map.insert(std::pair<sf::TcpSocket*, std::wstring>(client, DEFAULT_NAME));
                    std::cout << "����� ������ ��������� (" << playersinfo.clients_map.size() << " �����)" << std::endl;

                    // ��������� ����� ������� � ��������
                    selector.add(*client);
                }
                else
                {
                    std::cout << "������ ����������� �������" << std::endl;
                    delete client;
                }
            }
            else
            {
                // ������������ ������� ��� ������� �������
                for (auto it = playersinfo.clients_map.begin(); it != playersinfo.clients_map.end();)
                {
                    sf::TcpSocket* client = it->first;

                    if (selector.isReady(*client))
                    {
                        sf::Packet packet;
                        if (client->receive(packet) == sf::Socket::Done)
                        {
                            // ������ �������� �������, ������������ ��
                            // ��������� ������...
                            dataProcessing(client, packet, playersinfo);
                            ++it;
                        }
                        else
                        {
                            // ������ ��� ��������� ������ �� �������, ��������� ���
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
                    std::wcout << L"������: " << playersinfo.clients_map[playersinfo.clientsToDelete_vector[i]] << L" ��������" << std::endl;
                    playersinfo.disconnectingClient(playersinfo.clientsToDelete_vector[i]);
                    //delete playersinfo.clientsToDelete_vector[i];
                }
                playersinfo.clientsToDelete_vector.clear();
            }
        }
    }

    // ��������� ���������� � ���������
    std::map<sf::TcpSocket*, std::wstring>::iterator clientsIt;
    for (clientsIt = playersinfo.clients_map.begin(); clientsIt != playersinfo.clients_map.end(); clientsIt++)
    {
        sf::TcpSocket* client = clientsIt->first;
        client->disconnect();
        delete client;
    }
    return 0;
}