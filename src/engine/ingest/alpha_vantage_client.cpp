#include "alpha_vantage_client.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <array>

// For HTTP requests - using curl for simplicity
#ifdef __APPLE__
#include <curl/curl.h>
#endif

namespace trading
{
    namespace
    {
        // Callback for curl to write response data
        size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp)
        {
            size_t totalSize = size * nmemb;
            userp->append(static_cast<char *>(contents), totalSize);
            return totalSize;
        }

        // Simple JSON value extractor (basic implementation for demo)
        std::string extract_json_value(const std::string &json, const std::string &key)
        {
            std::string search_key = "\"" + key + "\":";
            size_t start = json.find(search_key);
            if (start == std::string::npos)
                return "";

            start += search_key.length();

            // Skip whitespace
            while (start < json.length() && std::isspace(json[start]))
                start++;

            if (start >= json.length())
                return "";

            // Handle quoted strings
            if (json[start] == '"')
            {
                start++; // Skip opening quote
                size_t end = json.find('"', start);
                if (end == std::string::npos)
                    return "";
                return json.substr(start, end - start);
            }

            // Handle unquoted values (numbers, etc.)
            size_t end = start;
            while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != ']')
            {
                end++;
            }

            std::string value = json.substr(start, end - start);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            return value;
        }
    }

    AlphaVantageClient::AlphaVantageClient(const std::string &api_key,
                                           const std::vector<std::string> &symbols)
        : api_key_(api_key), symbols_(symbols), running_(false), stop_requested_(false)
    {
        log_info("AlphaVantageClient created with " + std::to_string(symbols_.size()) + " symbols");

        // Initialize curl
        #ifdef __APPLE__
            curl_global_init(CURL_GLOBAL_DEFAULT);
        #endif
    }

    AlphaVantageClient::~AlphaVantageClient()
    {
        stop();

        #ifdef __APPLE__
                curl_global_cleanup();
        #endif
    }

    bool AlphaVantageClient::start()
    {
        if (running_.load())
        {
            return false;
        }

        if (api_key_.empty())
        {
            log_error("API key is required");
            return false;
        }

        if (symbols_.empty())
        {
            log_error("At least one symbol is required");
            return false;
        }

        stop_requested_.store(false);
        running_.store(true);

        // Start data fetching thread
        fetch_thread_ = std::thread(&AlphaVantageClient::data_fetch_loop, this);

        log_info("Alpha Vantage client started");
        return true;
    }

    void AlphaVantageClient::stop()
    {
        if (!running_.load())
        {
            return;
        }

        stop_requested_.store(true);
        running_.store(false);

        if (fetch_thread_.joinable())
        {
            fetch_thread_.join();
        }

        log_info("Alpha Vantage client stopped");
    }

    bool AlphaVantageClient::is_running() const
    {
        return running_.load();
    }

    void AlphaVantageClient::set_tick_callback(TickCallback callback)
    {
        tick_callback_ = callback;
    }

    void AlphaVantageClient::add_symbol(const std::string &symbol)
    {
        auto it = std::find(symbols_.begin(), symbols_.end(), symbol);
        if (it == symbols_.end())
        {
            symbols_.push_back(symbol);
            log_info("Added symbol: " + symbol);
        }
    }

    void AlphaVantageClient::set_polling_interval(std::chrono::seconds interval)
    {
        polling_interval_ = interval;
        log_info("Polling interval set to " + std::to_string(interval.count()) + " seconds");
    }

    AlphaVantageClient::LatencyMetrics AlphaVantageClient::get_latency_metrics() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return metrics_;
    }

    std::string AlphaVantageClient::make_http_request(const std::string &url) const
    {
        std::string response;

        #ifdef __APPLE__
            CURL *curl = curl_easy_init();
            if (!curl)
            {
                log_error("Failed to initialize curl");
                return "";
            }

            // Set URL
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Set callback for response data
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            // Set timeout
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

            // Follow redirects
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            // Perform request
            CURLcode res = curl_easy_perform(curl);

            if (res != CURLE_OK)
            {
                log_error("HTTP request failed: " + std::string(curl_easy_strerror(res)));
                response.clear();
            }

            curl_easy_cleanup(curl);
        #else
            // Fallback for non-macOS systems - use a simple implementation
            log_error("HTTP requests not implemented for this platform");

        #endif

        return response;
    }

    std::string AlphaVantageClient::build_api_url(const std::string &symbol) const
    {
        return "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=" + symbol + "&apikey=" + api_key_;
    }

    bool AlphaVantageClient::parse_time_series_response(const std::string &json_response, const std::string &symbol, std::vector<TickData> &ticks) const
    {
        if (json_response.empty())
        {
            return false;
        }

        // Check for error messages
        if (json_response.find("Error Message") != std::string::npos ||
            json_response.find("Invalid API call") != std::string::npos)
        {
            log_error("API error in response for " + symbol);
            return false;
        }

        // Check for rate limit message
        if (json_response.find("Thank you for using Alpha Vantage") != std::string::npos &&
            json_response.find("call frequency") != std::string::npos)
        {
            log_error("Rate limit exceeded for " + symbol);
            return false;
        }

        // Find Time Series section
        size_t time_series_pos = json_response.find("\"Time Series (Daily)\"");
        if (time_series_pos == std::string::npos)
        {
            log_error("No time series data found for " + symbol);
            return false;
        }

        // Extract the most recent date and its data
        // Look for the first date entry after "Time Series (Daily)"
        size_t date_start = json_response.find("\"20", time_series_pos);
        if (date_start == std::string::npos)
        {
            log_error("No date entries found for " + symbol);
            return false;
        }

        // Extract date
        size_t date_end = json_response.find("\"", date_start + 1);
        if (date_end == std::string::npos)
        {
            return false;
        }

        std::string date = json_response.substr(date_start + 1, date_end - date_start - 1);

        // Find the data block for this date
        size_t data_start = json_response.find("{", date_end);
        size_t data_end = json_response.find("}", data_start);
        if (data_start == std::string::npos || data_end == std::string::npos)
        {
            return false;
        }

        std::string data_block = json_response.substr(data_start, data_end - data_start + 1);

        // Extract OHLC data
        std::string open_str = extract_json_value(data_block, "1. open");
        std::string high_str = extract_json_value(data_block, "2. high");
        std::string low_str = extract_json_value(data_block, "3. low");
        std::string close_str = extract_json_value(data_block, "4. close");
        std::string volume_str = extract_json_value(data_block, "5. volume");

        if (open_str.empty() || close_str.empty() || volume_str.empty())
        {
            log_error("Failed to parse OHLC data for " + symbol);
            return false;
        }

        try
        {
            double open_price = std::stod(open_str);
            double high_price = std::stod(high_str);
            double low_price = std::stod(low_str);
            double close_price = std::stod(close_str);
            std::uint64_t volume = static_cast<std::uint64_t>(std::stoull(volume_str));

            // Extract exchange from symbol (e.g., "RELIANCE.BSE" -> "BSE")
            std::string clean_symbol = symbol;
            std::string exchange = "BSE";
            size_t dot_pos = symbol.find('.');
            if (dot_pos != std::string::npos)
            {
                clean_symbol = symbol.substr(0, dot_pos);
                exchange = symbol.substr(dot_pos + 1);
            }

            // Create tick data for open, high, low, close
            // We'll simulate this as separate trades
            auto now = std::chrono::system_clock::now();
            auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                    now.time_since_epoch())
                                    .count();

            // Create ticks for the day's trading activity
            // Open trade
            TickData open_tick(clean_symbol, exchange, open_price, volume / 4, 'B');
            open_tick.timestamp_us = timestamp_us;
            ticks.push_back(open_tick);

            // High trade (if different from open)
            if (high_price != open_price)
            {
                TickData high_tick(clean_symbol, exchange, high_price, volume / 4, 'B');
                high_tick.timestamp_us = timestamp_us + 1;
                ticks.push_back(high_tick);
            }

            // Low trade (if different from others)
            if (low_price != open_price && low_price != high_price)
            {
                TickData low_tick(clean_symbol, exchange, low_price, volume / 4, 'S');
                low_tick.timestamp_us = timestamp_us + 2;
                ticks.push_back(low_tick);
            }

            // Close trade
            TickData close_tick(clean_symbol, exchange, close_price, volume / 4,
                                close_price > open_price ? 'B' : 'S');
            close_tick.timestamp_us = timestamp_us + 3;
            ticks.push_back(close_tick);

            log_info("Parsed " + std::to_string(ticks.size()) + " ticks for " + symbol +
                     " (date: " + date + ", close: " + close_str + ")");

            return true;
        }
        catch (const std::exception &e)
        {
            log_error("Error parsing numeric data for " + symbol + ": " + e.what());
            return false;
        }
    }

    void AlphaVantageClient::data_fetch_loop()
    {
        log_info("Data fetch loop started");

        while (!stop_requested_.load())
        {
            // Process each symbol
            for (const auto &symbol : symbols_)
            {
                if (stop_requested_.load())
                {
                    break;
                }

                // Check rate limit
                if (!can_make_request())
                {
                    log_info("Rate limit reached, skipping requests");
                    break;
                }

                log_info("Fetching data for " + symbol);

                // Measure API call latency
                auto start_time = std::chrono::high_resolution_clock::now();

                // Make API request
                std::string url = build_api_url(symbol);
                std::string response = make_http_request(url);

                auto end_time = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time);

                // Record request for rate limiting
                record_request();

                // Update latency metrics
                {
                    std::lock_guard<std::mutex> lock(metrics_mutex_);
                    metrics_.request_count++;

                    if (response.empty())
                    {
                        metrics_.error_count++;
                    }
                    else
                    {
                        // Update latency stats
                        auto total_latency = metrics_.avg_api_latency.count() * (metrics_.request_count - 1);
                        metrics_.avg_api_latency = std::chrono::microseconds(
                            (total_latency + latency.count()) / metrics_.request_count);

                        if (latency > metrics_.max_api_latency)
                        {
                            metrics_.max_api_latency = latency;
                        }

                        if (latency < metrics_.min_api_latency)
                        {
                            metrics_.min_api_latency = latency;
                        }
                    }
                }

                // Parse response and generate ticks
                if (!response.empty())
                {
                    std::vector<TickData> ticks;
                    if (parse_time_series_response(response, symbol, ticks)) // parsed OHLC prices
                    {
                        // Send ticks to callback
                        for (auto &tick : ticks)
                        {
                            tick.receive_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                                       std::chrono::system_clock::now().time_since_epoch())
                                                       .count();

                            invoke_callback(tick);
                        }
                    }
                }

                // Wait between symbol requests to avoid overwhelming the API
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Wait for next polling cycle
            log_info("Completed polling cycle, waiting " + std::to_string(polling_interval_.count()) + " seconds");

            auto wait_start = std::chrono::steady_clock::now();
            while (!stop_requested_.load() &&
                   (std::chrono::steady_clock::now() - wait_start) < polling_interval_)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        log_info("Data fetch loop ended");
    }

    bool AlphaVantageClient::can_make_request() const
    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);

        auto now = std::chrono::system_clock::now();
        auto twenty_four_hours_ago = now - std::chrono::hours(24);

        // Count requests in the last 24 hours
        size_t recent_requests = 0;
        for (const auto &time : request_times_)
        {
            if (time > twenty_four_hours_ago)
            {
                recent_requests++;
            }
        }

        return recent_requests < MAX_REQUESTS_PER_DAY;
    }

    void AlphaVantageClient::record_request()
    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);

        auto now = std::chrono::system_clock::now();
        request_times_.push_back(now);

        // Clean up old entries (older than 24 hours)
        auto twenty_four_hours_ago = now - std::chrono::hours(24);
        request_times_.erase(
            std::remove_if(request_times_.begin(), request_times_.end(),
                           [twenty_four_hours_ago](const auto &time)
                           {
                               return time <= twenty_four_hours_ago;
                           }),
            request_times_.end());
    }

    void AlphaVantageClient::log_error(const std::string &message) const
    {
        std::cerr << "[AlphaVantage ERROR] " << message << std::endl;
    }

    void AlphaVantageClient::log_info(const std::string &message) const
    {
        std::cout << "[AlphaVantage INFO] " << message << std::endl;
    }

}