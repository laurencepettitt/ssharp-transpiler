#include <iostream>

typedef uint64_t number;

number read(){number x; std::cin >> x;return x;}
number write(number x){std::cout << x;return x;}
number test(number u,number v){return ([](){return [](){return (number)5;}(),[](){return (number)5;}();}());}
int main(){return ([](){return [](){return (number)5;}(),[](){return (number)5;}();}()),0;}
