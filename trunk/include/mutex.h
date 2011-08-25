#ifndef MUTEX_H
#define MUTEX_H


class Mutex
{
    public:
        Mutex();
        ~Mutex();
    protected:
        SDL_mutex *m_handle;
    private:
};

#endif // MUTEX_H
