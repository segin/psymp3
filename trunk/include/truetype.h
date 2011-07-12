#ifndef TRUETYPE_H
#define TRUETYPE_H


class TrueType : public Surface
{
    public:
        TrueType();
        virtual ~TrueType();
        static void Init(); // does not return on failure.
    protected:
    private:
};

#endif // TRUETYPE_H
