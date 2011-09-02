#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

// When no format can open the file.
class InvalidMediaException : public std::exception
{
    public:
        InvalidMediaException();
    protected:
    private:
};

// Correct format, but incorrect data for whatever reason (file corrupt, etc.)
class BadFormatException : public std::exception
{
    public:
        BadFormatException();
    protected:
    private:
};

// Not correct data format for this class. Try next or throw InvalidMediaException
class WrongFormatException : public std::exception
{
    public:
        WrongFormatException();
    protected:
    private:
};

#endif // EXCEPTIONS_H
