/**
  ******************************************************************************
  * @file   thread_queue.h
  * @author Alejandro Hontanilla Belinchón (a.hontanillab@alumnos.upm.es)
  * @brief  Thread-safe queue that provides synchronization
  *
  * @note   End-of-degree work.
  ******************************************************************************
*/

#ifndef SRC_THREAD_SIGNALS_THREAD_QUEUE_H_
#define SRC_THREAD_SIGNALS_THREAD_QUEUE_H_

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class Thread_queue {
private:

	// data queue
    std::queue<T> m_queue;

    // mutex for thread synchronization
    std::mutex m_mutex;

    // Condition variable for signaling
    std::condition_variable m_cond;

public:

	/**
	* @brief Pushes an element of type T to the queue.
	*/
    void push(T item){

    	// Acquire lock
    	std::unique_lock<std::mutex> lock(m_mutex);

    	// Add item
    	m_queue.push(item);

    	// Notify one thread that
    	// is waiting
    	m_cond.notify_one();
    }

	/**
	* @brief Pops the older element in the queue. If queue is empty it waits until an element is pushed.
	*
	* @return Element of type T.
	*/
    T pop()
    {

    	// acquire lock
    	std::unique_lock<std::mutex> lock(m_mutex);

    	// wait until queue is not empty
    	m_cond.wait(lock, [this]() { return !m_queue.empty(); });

    	// retrieve item
    	T item = m_queue.front();
    	m_queue.pop();

    	// return item
    	return item;
    }

	/**
	* @brief Pops the older element in the queue. If queue is empty it waits until an element is pushed or
	* 		 timeout is reached.
	*
	* @param[in] timeout timeout in milliseconds.
	*
	* @return Element of type T.
	*/
    T pop(int timeout)
    {

    	// acquire lock
    	std::unique_lock<std::mutex> lock(m_mutex);

    	// wait until queue is not empty
    	bool element = m_cond.wait_for(lock, std::chrono::milliseconds(timeout), [this]() { return !m_queue.empty(); });

    	// retrieve item
    	T item;
    	if(element){
    		item = m_queue.front();
    		m_queue.pop();
    	}else{
    		return -1;
    	}

    	// return item
    	return item;
    }

	/**
	* @brief Pops the oldest element in the queue without erasing it.
	* 		 If queue is empty it waits until an element is pushed.
	*
	* @return Element of type T.
	*/
    T front()
    {

    	// acquire lock
    	std::unique_lock<std::mutex> lock(m_mutex);

    	// wait until queue is not empty
    	m_cond.wait(lock, [this]() { return !m_queue.empty(); });

    	// retrieve item
    	T item = m_queue.front();

    	// return item
    	return item;
    }

	/**
	* @brief Pops the newest element in the queue without erasing it.
	* 		 If queue is empty it waits until an element is pushed.
	*
	* @return Element of type T.
	*/
    T back()
    {

    	// acquire lock
    	std::unique_lock<std::mutex> lock(m_mutex);

    	// wait until queue is not empty
    	m_cond.wait(lock, [this]() { return !m_queue.empty(); });

    	// retrieve item
    	T item = m_queue.back();

    	// return item
    	return item;
    }

	/**
	* @brief Pops the newest element in the queue and clears the whole queue.
	* 		 If queue is empty it waits until an element is pushed.
	*
	* @return Element of type T.
	*/
    T back_clear()
    {

    	// acquire lock
    	std::unique_lock<std::mutex> lock(m_mutex);

    	// wait until queue is not empty
    	m_cond.wait(lock, [this]() { return !m_queue.empty(); });

    	// retrieve item
    	T item = m_queue.back();

    	std::queue<T> empty;
    	m_queue.swap(empty);

    	// return item
    	return item;
    }

};


#endif /* SRC_THREAD_SIGNALS_THREAD_QUEUE_H_ */
