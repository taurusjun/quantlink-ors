Copyright(c) 2012 varun<email>

#ifndef CLIENTDATA_H
#define CLIENTDATA_H

#include <string>
#include <vector>

    struct ClientData
{
    vector<string> input_data;
    void *output_data;

    void clean_input()
    {
        input_data.erase();
    }

    void clear_output()
    {
        if (sizeof(output_data))
            output_data = 0;
    }
};

#endif // CLIENTDATA_H
