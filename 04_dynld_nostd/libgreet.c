// Copyright (c) 2020 Johannes Stoelp

int gCalled = 0;

const char* get_greet() {
    ++gCalled;
    return "Hello from libgreet.so!";
}

const char* get_greet2() {
    ++gCalled;
    return "Hello 2 from libgreet.so!";
}
