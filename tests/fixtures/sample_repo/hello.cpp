#include <iostream>
#include <string>

class Greeter {
public:
    void greet(const std::string& name) {
        std::cout << "Hello, " << name << "!\n";
    }
};

int add(int a, int b) {
    return a + b;
}

int main() {
    Greeter g;
    g.greet("world");
    return 0;
}
