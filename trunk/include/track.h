#ifndef TRACK_H
#define TRACK_H

class track
{
    public:
        track();
        track(std::string a_Artist, std::string a_Title, std::string a_Album, std::string a_FilePath, unsigned int a_Len);
        virtual ~track();
        track& operator=(const track& rhs);
        std::string GetArtist() { return m_Artist; }
        void SetArtist(std::string val) { m_Artist = val; }
        std::string GetTitle() { return m_Title; }
        void SetTitle(std::string val) { m_Title = val; }
        std::string GetAlbum() { return m_Album; }
        void SetAlbum(std::string val) { m_Album = val; }
        std::string GetFilePath() { return m_FilePath; }
        void SetFilePath(std::string val) { m_FilePath = val; }
        unsigned int GetLen() { return m_Len; }
        void SetLen(unsigned int val) { m_Len = val; }
    protected:
    private:
        std::string m_Artist;
        std::string m_Title;
        std::string m_Album;
        std::string m_FilePath;
        unsigned int m_Len;
};

#endif // TRACK_H
