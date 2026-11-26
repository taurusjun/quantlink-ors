#include <cstring>
#include <functional>
#include <string>

namespace illuminati
{
    namespace ds
    {
        class cstring
        {
        public:
            cstring()
            {
            }
            cstring(cstring &&c)
            {
                _str = c._str;
            }

            cstring(cstring &c)
            {
                _str = c._str;
            }

            cstring(const cstring &c)
            {
                _str = c._str;
            }
            explicit cstring(const char *c)
            {
                _str = c; // = new char[std::strlen(c) +1];
                          // strcpy(_str, c);
            }

            explicit cstring(const char *c, size_t len)
            {
                _str = new char[len + 1];
                memcpy((void *)_str, c, len);
                *(((char *)_str) + len) = '\0';
            }

            const char *c_str() const
            {
                return _str;
            }

            cstring clone()
            {
                cstring cpy;
                cpy._str = new char[std::strlen(_str) + 1];
                strcpy((char *)(cpy._str), _str);
                return cpy;
            }

            bool operator==(const cstring &rhs) const
            {
                return std::strcmp(_str, rhs._str) == 0;
            }

            bool operator<(const cstring rhs) const
            {
                return std::strcmp(_str, rhs._str) < 0;
            }

            friend bool operator==(cstring lhs, const std::string &rhs);
            friend bool operator==(const std::string &lhs, cstring rhs);
            friend bool operator==(cstring lhs, const char *rhs);
            friend bool operator==(const char *lhs, cstring rhs);

        protected:
            const char *_str;
        };

        bool operator==(cstring lhs, const std::string &rhs)
        {
            return std::strcmp(lhs._str, rhs.c_str()) == 0;
        }
        bool operator==(const std::string &lhs, cstring rhs)
        {
            return std::strcmp(lhs.c_str(), rhs._str) == 0;
        }

        bool operator==(cstring lhs, const char *rhs)
        {
            return std::strcmp(lhs._str, rhs) == 0;
        }

        bool operator==(const char *lhs, cstring rhs)
        {
            return std::strcmp(lhs, rhs._str) == 0;
        }
    }
}

namespace std
{
    // djb2 hash function
    template <>
    struct hash<illuminati::ds::cstring>
    {
        size_t operator()(const illuminati::ds::cstring &s) const
        {
            size_t hash = 5381;
            const char *str = s.c_str();
            int c;

            while (c = *str++)
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

            return hash;
        }
    };
}
