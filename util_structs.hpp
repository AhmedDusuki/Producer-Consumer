#pragma once

#define NAME_LENGTH 11 // including null terminator

struct record
{
    int turn;
    char prod_name[NAME_LENGTH];
    double price;
};

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;