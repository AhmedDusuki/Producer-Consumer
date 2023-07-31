#include <iostream>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include "util_structs.hpp"

std::string truncate(std::string str, size_t width)
{
    if (str.length() > width)
    {
        return str.substr(0, width);
    }
    return str;
}

inline void get_timestamp(char *timestamp_buffer, size_t buf_size)
{
    auto timepoint = std::chrono::system_clock::now();
    auto coarse = std::chrono::system_clock::to_time_t(timepoint);
    auto fine = std::chrono::time_point_cast<std::chrono::milliseconds>(timepoint);

    std::snprintf(timestamp_buffer + std::strftime(timestamp_buffer, buf_size - 4,
                                                   "[%m/%d/%Y %H:%M:%S.", std::localtime(&coarse)),
                  5, "%03lu]", fine.time_since_epoch().count() % 1000);
}

void append_product(void *shm_add, std::string name, double price, int buffer_size)
{
    int n;
    record *rec = static_cast<record *>(shm_add);
    n = rec->turn;  // turn number of current product
    rec->turn += 1; // increment products count
    rec++;
    for (int i = 0; i < buffer_size; ++i)
    { // place product in first empty spot
        if (rec->turn == -1)
        {
            rec->turn = n;
            memset(rec->prod_name, 0, NAME_LENGTH);
            char *c = rec->prod_name;
            for (int j = 0; j < name.length(); ++j)
            {
                *c = name[j];
                ++c;
            }
            rec->price = price;
            break;
        }
        rec++;
    }
}

int main(int argc, char *argv[])
{
    std::string commodity_name;
    double commodity_price_mean;
    double commodity_price_sd;
    int sleep_time_ms;
    int buffer_size;

    if (argc != 6)
    {
        std::cout << "usage: " << argv[0] << " <commodity name> <commodity price mean> <commodity price sd> <sleep time ms> <buffer size>\n";
        return EXIT_FAILURE;
    }
    else
    {
        commodity_name = truncate(argv[1], NAME_LENGTH - 1);

        commodity_price_mean = atof(argv[2]);

        commodity_price_sd = atof(argv[3]);

        sleep_time_ms = atoi(argv[4]);

        buffer_size = atoi(argv[5]);

        if (sleep_time_ms < 1 || buffer_size < 1)
        {
            std::cout << "sleep time and buffer size must be positive integers\n";
            return EXIT_FAILURE;
        }
    }

    std::default_random_engine generator;
    std::normal_distribution<double> distribution(commodity_price_mean, commodity_price_sd);

    size_t record_size = sizeof(record);
    size_t shm_size = (buffer_size + 1) * record_size; // first record contains number of products in buffer

    // ftok to generate unique key
    key_t shm_key = ftok("consumer", 65);

    // shmget returns an identifier in shmid
    int shmid = shmget(shm_key, shm_size, 0666);
    if (shmid == -1)
    {
        return EXIT_FAILURE;
    }

    void *shm_address = shmat(shmid, NULL, 0);

    // ftok to generate unique key
    key_t sem_key = ftok("producer", 65);

    // 0 is mutex for shared mem, 1 is filled spaces, 2 is for free spaces
    unsigned short sem_s = 0;
    unsigned short sem_n = 1;
    unsigned short sem_e = 2;
    int sem_id = semget(sem_key, 3, 0666);
    if (sem_id == -1)
    {
        return EXIT_FAILURE;
    }

    sembuf sop;

    char timestamp_buffer[sizeof("[12/31/2022 23:59:59.999]")];
    size_t timestamp_buffer_size = sizeof(timestamp_buffer);

    std::cerr << std::fixed;
    std::cerr << std::setprecision(2);

    while (true)
    {

        double price = distribution(generator);

        get_timestamp(timestamp_buffer, timestamp_buffer_size);
        std::cerr << timestamp_buffer << " " << commodity_name << ": generating a new value " << price << '\n';

        sop = {sem_e, -1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }

        get_timestamp(timestamp_buffer, timestamp_buffer_size);
        std::cerr << timestamp_buffer << " " << commodity_name << ": trying to get mutex on shared buffer\n";

        sop = {sem_s, -1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }

        get_timestamp(timestamp_buffer, timestamp_buffer_size);
        std::cerr << timestamp_buffer << " " << commodity_name << ": placing " << price << " on shared buffer\n";
        append_product(shm_address, commodity_name, price, buffer_size);

        sop = {sem_s, 1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }
        sop = {sem_n, 1, SEM_UNDO};
        if (semop(sem_id, &sop, 1) == -1)
        {
            return EXIT_FAILURE;
        }

        get_timestamp(timestamp_buffer, timestamp_buffer_size);
        std::cerr << timestamp_buffer << " " << commodity_name << ": sleeping for " << sleep_time_ms << " ms" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
    }

    return 0;
}