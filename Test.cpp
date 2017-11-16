#include <iostream>
#include <thread>

#include "MessageController.h"

namespace Test {
    using namespace std;

    struct Wrapper {
        Wrapper() noexcept  {
            cout << "------wrapper constructed!------" << endl;
        }

        Wrapper(const Wrapper&) noexcept {
            cout << "------wrapper copied!------" << endl;
        }

        Wrapper(Wrapper&&) noexcept {
            cout << "------wrapper moved!------" << endl;
        }
    };

    struct Object {
        void signal(int, int) {}

        void slot0(char c, const Wrapper&) {
            cout << "object slot0: c = " << c << endl;
        }

        void slot1(int x, int y) {
            cout << "object slot1: x " << x << ", y = " << y << endl;
        }
    };

    void signal0(int, char) {}

    void signal1(char, Wrapper) {}

    void signal2(string , string) {}


    void slot0(int x, char c) {
        cout << "slot0: x = " << x << ", c = " << c << endl;
    }

    void slot1(int x, char c) {
        cout << "slot1: x = " << x << ", c = " << c << endl;
    }

    void slot2(char c, const Wrapper&) {
        cout << "slot2: c = " << c << endl;
    }

    void slot3(string first, string second) {
        cout << "slot3: first = " << first << ", second = " << second << endl;
    }

    void slot4(const string& first, const string& second) {
        cout << "slot4: first = " << first << ", second = " << second << endl;
    }

    void slot5(int x, int y) {
        cout << "slot5: x = " << x << ", y = " << y << endl;
    }

    void test() {
        MessageController controller;
        thread server(&MessageController::start, &controller);

        Object object;
        Wrapper wrapper;

        controller.connect(&signal0, &slot0);
        controller.connect(&signal0, &slot1);

        controller.connect(&signal1, &slot2);

        controller.connect(&signal2, &slot3);
        controller.connect(&signal2, &slot4);


        controller.connect(&signal0, [](int x, char c){
           cout << "lambda slot0: x = " << x << ", c = " << c << endl;
        });

        controller.connect(&signal1, [](char c, const Wrapper&){
            cout << "lambda slot1: c = " << c << endl;
        });

        controller.connect(&signal1, [](char c, const Wrapper&){
            cout << "lambda slot2: c = " << c << endl;
        });

        controller.connect(&signal2, [](string first, string second){
            cout << "lambda slot3: first = " << first
                      << ", second = " << second << endl;
        });

        controller.connect(&signal2, [](const string& first, const string& second){
            cout << "lambda slot4: first = " << first
                      << ", second = " << second << endl;
        });

        controller.connect(&Object::signal, &object, &slot5);

        controller.connect(&Object::signal, &object, [=](int x, int y){
            cout << "lambda slot5: x = " << x
                      << ", y =" << y << endl;
        });

        controller.connect(&Object::signal, &object, [=](int, int){
            cout << "lambda slot6" << endl;
        });

        controller.connect(&Object::signal, &object, &Object::slot1, &object);


        controller.emit(&Object::signal, &object, 32, 43);
        controller.emit(&Object::signal, &object, 99, 4);

        controller.emit(&signal0, 87, 'x');
        controller.emit(&signal1, 'd', move(wrapper));
        controller.emit(&signal2, string("peter"), string("list"));

        controller.stop();
        server.join();
    }
}


int main()
{
    Test::test();

    return 0;
}
