#ifndef RECT_H
#define RECT_H


class Rect
{
    public:
        Rect();
        virtual ~Rect();
        unsigned int width() { return m_width; };
        unsigned int height() { return m_height; };
        void width(unsigned int a) { m_width = a; };
        void height(unsigned int a) { m_height = a; };
    protected:
    private:
        unsigned int m_width;
        unsigned int m_height;
};

#endif // RECT_H
