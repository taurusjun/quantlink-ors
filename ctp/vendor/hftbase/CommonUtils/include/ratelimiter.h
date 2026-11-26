#ifndef RATELIMITER_H_
#define RATELIMITER_H_

namespace illuminati
{
    namespace infra
    {

        class RateLimiter
        {
        public:
            RateLimiter()
                : m_tokens_available(0)
            {
            }

            RateLimiter(
                double investment_value,
                uint64_t time_range)
                : m_token_rate(investment_value / time_range), m_bucket_capacity(investment_value), m_tokens_available(investment_value), m_last_timestamp(0)
            {
            }

            bool check_incoming_investment_rate(double investment, uint64_t timestamp) const
            {
                double tokens_available = std::min(m_bucket_capacity, m_tokens_available + (timestamp - m_last_timestamp) * m_token_rate);
                if (investment > tokens_available)
                    return false;
                else
                    return true;
            }
            bool update_investment_rate_state(double investment, uint64_t timestamp)
            {
                m_tokens_available = std::min(m_bucket_capacity, m_tokens_available + (timestamp - m_last_timestamp) * m_token_rate);
                if (investment > m_tokens_available)
                    return false;
                else
                {
                    m_tokens_available = m_tokens_available - investment;
                    m_last_timestamp = timestamp;
                }
                return true;
            }

        private:
            double m_token_rate;
            double m_bucket_capacity;
            double m_tokens_available;
            uint64_t m_last_timestamp;
        };
    } // namespace infra
} // namespace illuminati
#endif
