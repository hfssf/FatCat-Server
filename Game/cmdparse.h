#ifndef CMDPARSE_H
#define CMDPARSE_H

#include "./../NetWork/tcpconnection.h"


void CommandParse(TCPConnection::Pointer conn, hf_char* reg);

void CommandParseLogin(TCPConnection::Pointer conn, hf_char* reg);

#endif  // CMDPARSE_H
