#ifndef _READSTATES_H
#define _READSTATES_H

enum ReadStates
{
    READ_SUCC = 1,
    READ_ERR,
    FILE_EOF
};

enum AdditionalDataParseStates
{
    UNKNOWN_SEQUENCE_NUMBER = 0,
    PARSER_ERROR,
    PACKET_FOUND
};

#endif
