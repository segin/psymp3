#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

class InvalidMediaException : public std::exception
{
    public:
        /** Default constructor */
        InvalidMediaException();
    protected:
    private:
};

#endif // EXCEPTIONS_H
