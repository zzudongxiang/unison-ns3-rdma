/*
 * Copyright (c) 2012 Telum (www.telum.ru)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Kirill Andreev <andreev@telum.ru>
 */
#ifndef PROPAGATION_CACHE_H_
#define PROPAGATION_CACHE_H_

#include "ns3/mobility-model.h"

#include <map>

namespace ns3
{
/**
 * \ingroup propagation
 * \brief Constructs a cache of objects, where each object is responsible for a single propagation
 * path loss calculations. Propagation path a-->b and b-->a is the same thing. Propagation path is
 * identified by a couple of MobilityModels and a spectrum model UID
 */
template <class T>
class PropagationCache
{
  public:
    PropagationCache(){};
    ~PropagationCache(){};

    /**
     * Get the model associated with the path
     * \param a 1st node mobility model
     * \param b 2nd node mobility model
     * \param modelUid model UID
     * \return the model
     */
    Ptr<T> GetPathData(Ptr<const MobilityModel> a, Ptr<const MobilityModel> b, uint32_t modelUid)
    {
        PropagationPathIdentifier key = PropagationPathIdentifier(a, b, modelUid);
        auto it = m_pathCache.find(key);
        if (it == m_pathCache.end())
        {
            return nullptr;
        }
        return it->second;
    }

    /**
     * Add a model to the path
     * \param data the model to associate to the path
     * \param a 1st node mobility model
     * \param b 2nd node mobility model
     * \param modelUid model UID
     */
    void AddPathData(Ptr<T> data,
                     Ptr<const MobilityModel> a,
                     Ptr<const MobilityModel> b,
                     uint32_t modelUid)
    {
        PropagationPathIdentifier key = PropagationPathIdentifier(a, b, modelUid);
        NS_ASSERT(m_pathCache.find(key) == m_pathCache.end());
        m_pathCache.insert(std::make_pair(key, data));
    }

    /**
     * Clean the cache
     */
    void Cleanup()
    {
        for (auto i : m_pathCache)
        {
            i.second->Dispose();
        }
        m_pathCache.clear();
    }

  private:
    /// Each path is identified by
    struct PropagationPathIdentifier
    {
        /**
         * Constructor
         * @param a 1st node mobility model
         * @param b 2nd node mobility model
         * @param modelUid model UID
         */
        PropagationPathIdentifier(Ptr<const MobilityModel> a,
                                  Ptr<const MobilityModel> b,
                                  uint32_t modelUid)
            : m_srcMobility(a),
              m_dstMobility(b),
              m_spectrumModelUid(modelUid){};
        Ptr<const MobilityModel> m_srcMobility; //!< 1st node mobility model
        Ptr<const MobilityModel> m_dstMobility; //!< 2nd node mobility model
        uint32_t m_spectrumModelUid;            //!< model UID

        /**
         * Less-than operator.
         *
         * The goal of this operator is just to provide a stable comparison
         * to be used in containers requiring a order (of any kind).
         *
         * If the models are different, the comparison is based on their Uid.
         * Otherwise, the comparison is based on the pointers of the Mobility models.
         *
         * \param other Right value of the operator.
         * \returns True if the Left value is less than the Right value.
         */
        bool operator<(const PropagationPathIdentifier& other) const
        {
            if (m_spectrumModelUid != other.m_spectrumModelUid)
            {
                return m_spectrumModelUid < other.m_spectrumModelUid;
            }
            /// Links are supposed to be symmetrical!
            if (std::min(m_dstMobility, m_srcMobility) !=
                std::min(other.m_dstMobility, other.m_srcMobility))
            {
                return std::min(m_dstMobility, m_srcMobility) <
                       std::min(other.m_dstMobility, other.m_srcMobility);
            }
            if (std::max(m_dstMobility, m_srcMobility) !=
                std::max(other.m_dstMobility, other.m_srcMobility))
            {
                return std::max(m_dstMobility, m_srcMobility) <
                       std::max(other.m_dstMobility, other.m_srcMobility);
            }
            return false;
        }
    };

    /// Typedef: PropagationPathIdentifier, Ptr<T>
    typedef std::map<PropagationPathIdentifier, Ptr<T>> PathCache;

  private:
    PathCache m_pathCache; //!< Path cache
};
} // namespace ns3

#endif // PROPAGATION_CACHE_H_
