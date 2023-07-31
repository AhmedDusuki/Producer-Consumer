#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <cmath>
#include <signal.h>
#include "util_structs.hpp"

enum COLOR
{
    blue,
    green,
    red
};

enum ARROW
{
    none,
    up,
    down
};

int shmid, sem_id;
void *shm_address;

void signal_callback_handler(int signum)
{
    // destroy the semaphores
    semctl(sem_id, 0, IPC_RMID, 0);

    // detach from shared memory
    shmdt(shm_address);

    // destroy the shared memory
    shmctl(shmid, IPC_RMID, NULL);
}

void take_product(void *shm_add, int buffer_size, std::vector<std::pair<std::string, std::vector<double>>> &products_live)
{
    record *rec = static_cast<record *>(shm_add);
    rec->turn -= 1; // decrement products count
    rec++;
    std::string prod_name;
    double price;
    for (int i = 0; i < buffer_size; ++i)
    { // take product who's turn is 0 and decrement other's turn
        if (rec->turn == -1)
        {
            rec++;
            continue;
        }
        if (rec->turn == 0)
        {
            prod_name.append(rec->prod_name);
            price = rec->price;
        }
        rec->turn -= 1;
        rec++;
    }

    for (int i = 0; i < products_live.size(); ++i)
    {
        auto prod = products_live[i];
        if (prod.first == prod_name)
        {
            int n = prod.second.size();
            products_live[i].second.emplace_back(price);

            if (n == 8)
            {
                products_live[i].second.erase(products_live[i].second.begin() + 2);
            }

            // update 2nd last and last average price
            auto prices = products_live[i].second;
            auto const prices_count_double = static_cast<double>(prices.size() - 2); // average not included
            double average_price = std::reduce(prices.begin() + 2, prices.end()) / prices_count_double;
            products_live[i].second[0] = products_live[i].second[1];
            products_live[i].second[1] = average_price;
            break;
        }
    }
}

void print_double(double num, COLOR color, ARROW arrow)
{
    int floored_price = int(num);
    int cnt = 0;

    while (floored_price)
    {
        ++cnt;
        floored_price /= 10;
    }

    std::string color_start, color_end;

    switch (color)
    {
    case COLOR::blue:
        color_start = "\033[0;36m";
        color_end = "\033[0m";

        break;
    case COLOR::green:
        color_start = "\033[0;32m";
        color_end = "\033[0m";

        break;
    case COLOR::red:
        color_start = "\033[0;31m";
        color_end = "\033[0m";

        break;
    default:
        color_start = "\033[0;37m";
        color_end = "\033[0m";
        break;
    }

    switch (arrow)
    {
    case ARROW::none:
        color_end.insert(0, "  ");
        break;

    case ARROW::up:
        color_end.insert(0, "↑ ");
        break;

    case ARROW::down:
        color_end.insert(0, "↓ ");
        break;

    default:
        color_end.insert(0, "  ");
        break;
    }

    if (cnt == 0 || cnt == 1)
    {
        std::cout << "    ";
        std::cout << color_start << int(num);
    }
    else
    {
        int spaces = 5 - cnt; // 8 spaces - decimal point and 2 decimal digits - non decimal digits
        for (int i = 0; i < spaces; ++i)
        {
            std::cout << " ";
        }
        std::cout << color_start << int(num);
    }

    num = std::round(num * 100.0);

    std::cout << ".";
    int dec = int(num) % 100;
    if (dec >= 10)
    {
        std::cout << dec << color_end;
    }
    else
    {
        std::cout << "0" << dec << color_end;
    }

    std::cout << "|";
}

void print_table(std::vector<std::pair<std::string, std::vector<double>>> const &products_live)
{
    printf("\e[1;1H\e[2J");
    std::cout << "+-------------------------------------+\n";
    std::cout << "| Currency      |  Price   | AvgPrice |\n";
    std::cout << "+-------------------------------------+\n";
    for (int j = 0; j < products_live.size(); ++j)
    {
        auto product = products_live[j];
        int spaces = 14 - product.first.length();
        std::cout << "| " << product.first;
        for (int i = 0; i < spaces; ++i)
        {
            std::cout << " ";
        }
        std::cout << "|";
        std::vector<double> prices = product.second;
        int prices_count = prices.size();
        double latest_price = prices[prices_count - 1];

        COLOR color;
        ARROW arrow;

        if (prices_count == 3)
        {
            color = COLOR::green;
            arrow = ARROW::up;
        }
        else if (prices_count > 3)
        {
            if (prices[prices_count - 1] > prices[prices_count - 2])
            {
                color = COLOR::green;
                arrow = ARROW::up;
            }
            else if (prices[prices_count - 1] < prices[prices_count - 2])
            {
                color = COLOR::red;
                arrow = ARROW::down;
            }
            else
            {
                color = COLOR::blue;
                arrow = ARROW::none;
            }
        }
        else
        {
            color = COLOR::blue;
            arrow = ARROW::none;
        }

        print_double(latest_price, color, arrow);

        if (prices[1] > prices[0])
        {
            color = COLOR::green;
            arrow = ARROW::up;
        }
        else if (prices[1] < prices[0])
        {
            color = COLOR::red;
            arrow = ARROW::down;
        }
        else
        {
            color = COLOR::blue;
            arrow = ARROW::none;
        }

        print_double(prices[1], color, arrow);

        std::cout << std::endl;
    }
    std::cout << "+-------------------------------------+\n";
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_callback_handler);

    std::vector<std::string> products{"GOLD", "SILVER", "CRUDEOIL", "NATURALGAS", "ALUMINIUM", "COPPER", "NICKEL", "LEAD", "ZINC", "MENTHAOIL", "COTTON"};
    std::sort(products.begin(), products.end());

    int n;
    if (argc != 2)
    {
        std::cout << "usage: " << argv[0] << " <buffer size>\n";
        return EXIT_FAILURE;
    }
    else
    {
        n = atoi(argv[1]);
        if (n < 1)
        {
            std::cout << "usage: " << argv[0] << " <positive int buffer size>\n";
            return EXIT_FAILURE;
        }
    }

    size_t record_size = sizeof(record);
    size_t shm_size = (n + 1) * record_size; // first record contains number of products in buffer

    // ftok to generate unique key
    key_t shm_key = ftok("consumer", 65);

    // shmget returns an identifier in shmid
    shmid = shmget(shm_key, shm_size, IPC_CREAT | 0666);
    if (shmid == -1)
    {
        return EXIT_FAILURE;
    }

    shm_address = shmat(shmid, NULL, 0);

    // initialize first record with the turn = 0, the number of present products in buffer
    record *rec = static_cast<record *>(shm_address);
    rec->turn = 0;
    rec++;
    // initialize other records with turn = -1
    for (int i = 0; i < n; ++i)
    {
        rec->turn = -1;
        rec++;
    }

    // ftok to generate unique key
    key_t sem_key = ftok("producer", 65);

    // 0 is mutex for shared mem, 1 is filled spaces, 2 is for free spaces
    unsigned short sem_s = 0;
    unsigned short sem_n = 1;
    unsigned short sem_e = 2;

    sem_id = semget(sem_key, 3, IPC_CREAT | 0666);
    if (sem_id == -1)
    {
        return EXIT_FAILURE;
    }

    semun _un;
    _un.val = 1;
    semctl(sem_id, sem_s, SETVAL, _un);
    _un.val = 0;
    semctl(sem_id, sem_n, SETVAL, _un);
    _un.val = n;
    semctl(sem_id, sem_e, SETVAL, _un);

    std::vector<std::pair<std::string, std::vector<double>>> products_live((int)products.size(), std::pair(std::string(), std::vector(2, (double)0)));
    // 2nd last avg price, last avg price, 5th newest price, 4th newest price, ..., 1st newest price
    // max 7 prices
    for (int i = 0; i < products.size(); ++i)
    {
        products_live[i].first = products[i];
    }

    sembuf sop;

    print_table(products_live);

    while (true)
    {
        sop = {sem_n, -1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }
        sop = {sem_s, -1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }

        take_product(shm_address, n, products_live);

        sop = {sem_s, 1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }
        sop = {sem_e, 1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }

        print_table(products_live);
    }

    // destroy the semaphores
    semctl(sem_id, 0, IPC_RMID, 0);

    // detach from shared memory
    shmdt(shm_address);

    // destroy the shared memory
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}