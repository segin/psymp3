#ifndef MUTEX_H
#define MUTEX_H


class Mutex
{
    public:
        Mutex();
        ~Mutex();
        void lock();
        void unlock();
    protected:
        SDL_mutex *m_handle;
    private:
};

#endif // MUTEX_H
