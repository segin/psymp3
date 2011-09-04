#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

// When no format can open the file.
class InvalidMediaException : public std::exception
{
    public:
        InvalidMediaException(TagLib::String why);
        virtual const char *what();
        ~InvalidMediaException() throw ();
    protected:
    private:
    TagLib::String m_why;
};

// Correct format, but incorrect data for whatever reason (file corrupt, etc.)
class BadFormatException : public std::exception
{
    public:
        BadFormatException(TagLib::String why);
        ~BadFormatException() throw ();
        virtual const char *what();
    protected:
    private:
    TagLib::String m_why;
};

// Not correct data format for this class. Try next or throw InvalidMediaException
class WrongFormatException : public std::exception
{
    public:
        WrongFormatException(TagLib::String why);
        ~WrongFormatException() throw ();
        virtual const char *what();
    protected:
    private:
    TagLib::String m_why;
};

#endif // EXCEPTIONS_H
