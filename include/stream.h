#ifndef STREAM_H
#define STREAM_H


class Stream
{
    public:
        Stream();
        Stream(TagLib::String name);
        /** Default destructor */
        virtual ~Stream();
        virtual void open(TagLib::String name);
        TagLib::String getArtist();
        TagLib::String getTitle();
        TagLib::String getAlbum();
        unsigned int getLength(); // in msec!

    protected:
        void            *m_handle; // any handle type
    private:
        TagLib::String  m_path;
        TagLib::FileRef *m_tags;
        unsigned int    m_rate;

};

#endif // STREAM_H
