#ifndef LIBMPG123_H
#define LIBMPG123_H


class Libmpg123 : public Stream
{
    public:
        /** Default constructor */
        Libmpg123(TagLib::String name);
        /** Default destructor */
        virtual ~Libmpg123();
        virtual void open(TagLib::String name);
    protected:
    private:

};

#endif // LIBMPG123_H
