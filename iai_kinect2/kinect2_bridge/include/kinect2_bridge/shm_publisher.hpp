#ifndef __SHM_PUBLISHER_HPP__
#define __SHM_PUBLISHER_HPP__

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/atomic/atomic.hpp>
#include "ros/ros.h"

namespace shm_transport {

  class Topic;

  class Publisher {
    public:
      Publisher() : pub_(), pshm_(0) {
      }
      Publisher(const Publisher & p) {
        pub_ = p.pub_;
        pshm_ = p.pshm_;
        if (pshm_) {
          boost::atomic<uint32_t> * ref_ptr = pshm_->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
          ref_ptr->fetch_add(1, boost::memory_order_relaxed);
        }
      }
      void operator = (const Publisher & p) {
        pub_ = p.pub_;
        pshm_ = p.pshm_;
        if (pshm_) {
          boost::atomic<uint32_t> * ref_ptr = pshm_->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
          ref_ptr->fetch_add(1, boost::memory_order_relaxed);
        }
      }
    private:
      Publisher(const ros::Publisher & pub, boost::interprocess::managed_shared_memory * pshm) {
        pub_ = boost::make_shared< ros::Publisher >(pub);
        pshm_ = pshm;
        boost::atomic<uint32_t> * ref_ptr = pshm_->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
        ref_ptr->fetch_add(1, boost::memory_order_relaxed);
      }

    public:
      ~Publisher() {
        if (pshm_) {
          boost::atomic<uint32_t> * ref_ptr = pshm_->find_or_construct<boost::atomic<uint32_t> >("ref")(0);
          if (ref_ptr->fetch_sub(1, boost::memory_order_relaxed) == 1) {
            boost::interprocess::shared_memory_object::remove(pub_->getTopic().c_str());
            ROS_INFO("shm file <%s> removed\n", pub_->getTopic().c_str());
            if (pub_)
              shutdown();
            delete pshm_;
          }
        }
      }

      template < class M >
      void publish(const M & msg) const {
        if (!pshm_)
        {
          return;
        }
        if (pub_->getNumSubscribers() == 0)
        {
          return;
        }

        uint32_t serlen = ros::serialization::serializationLength(msg);
        uint32_t * ptr = (uint32_t *)pshm_->allocate(sizeof(uint32_t) * 2 + serlen);
        ptr[0] = pub_->getNumSubscribers();
        ptr[1] = serlen;
        ros::serialization::OStream out((uint8_t *)(ptr + 2), serlen);
        ros::serialization::serialize(out, msg);
        std_msgs::UInt64 actual_msg;
        actual_msg.data = pshm_->get_handle_from_address(ptr);
        pub_->publish(actual_msg);
      }

      void shutdown() {
        pub_->shutdown();
      }

      std::string getTopic() const {
        return pub_->getTopic();
      }

      uint32_t getNumSubscribers() const {
        return pub_->getNumSubscribers();
      }

    protected:
      boost::shared_ptr< ros::Publisher > pub_;
      boost::interprocess::managed_shared_memory * pshm_;

    friend class Topic;
  };

}

#endif // __SHM_PUBLISHER_HPP__

