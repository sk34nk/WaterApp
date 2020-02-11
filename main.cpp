#include <iostream>

#include "createApp.hpp"

using namespace std;
using namespace app;

int main()
{
    application app;
    try
    {
        app.run();
    }
    catch (const exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
