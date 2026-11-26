#ifndef MAILER_H
#define MAILER_H

#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include "stringutils.h"

const int MAXMAILSPERPROCESS = 5; // we don't want to spam

namespace illuminati
{
    namespace mail
    {
        /*!
         * Alert sender - Send only very critical mail through this func
         * mailx format taken from tradebot
         * @param subject mail subject
         * @param msg mail message
         * @param to delimited [,; ] mail receivers, comma makes this compatible with sendmail
         * @return return value of the command executed by system
         * example: check the usage in TestMailer.cpp
         */
        static int sendMailAlert(std::string subject, std::string msg, std::string to)
        {
            static int count = 0;

            illuminati::trim(to);
            illuminati::replaceAll(to, ";", " ");
            illuminati::replaceAll(to, ",", " ");

            if (to.empty())
            {
                to = "tradingsystems@way2wealth.com";
            }

            int retValue = 0;
            if (MAXMAILSPERPROCESS > count++)
            {
                std::string lastMessage = "";
                if (MAXMAILSPERPROCESS == count)
                {
                    lastMessage = "-=Last email for this process=-";
                }
                pid_t pid = getpid();
                std::stringstream ss;
                ss << "echo \"Host=`hostname`" << " pid=" << pid << "\nmessage=" << msg << "\n\""
                   << " | mailx -s \"INFRA Alert " << count << lastMessage << "!!! " << subject << "\""
                   << " -aFrom:tradingsystems@way2wealth.com "
                   << to;

                retValue = system(ss.str().c_str());
            }
            return retValue;
        }
    }
}

#endif
