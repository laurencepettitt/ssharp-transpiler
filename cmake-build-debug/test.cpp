#include <iostream>

typedef uint64_t number;

number read(){number x; std::cin >> x;return x;}
number write(number x){std::cout << x << std::endl;return x;}
int main(){return ([&](){return write(([&](){return write((number)3),(number)4;}()));}()),0;}
