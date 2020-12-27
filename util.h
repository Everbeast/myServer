
// #ifndef UTIL_H
// #define UTIL_H
#pragma once


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <signal.h>


void addsig( int sig, void( handler )(int), bool restart = true);


int setnonblocking(int fd);

void show_error( int connfd, const char* info );

void addfd(int epollfd, int fd, bool one_shot);

void removefd(int epollfd, int fd);


void modfd(int epollfd, int fd, int ev);


// #endif