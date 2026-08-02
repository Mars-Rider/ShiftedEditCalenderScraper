#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>

struct Event {
    std::string title;
    std::string date_str;     // e.g., "Jul 3"
    std::string time_str;     // e.g., "7:00 PM"
    std::string location;     // e.g., "Main Stage"
    std::string start_time;   // Formatted as YYYYMMDDTHHMMSS
    std::string end_time;     // Formatted as YYYYMMDDTHHMMSS
};

// 1. Download HTML using macOS/Linux built-in curl
std::string fetchHTML(const std::string& url) {
    std::string command = "curl -s \"" + url + "\" > temp_page.html";
    system(command.c_str());

    std::ifstream file("temp_page.html");
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    system("rm -f temp_page.html");
    return buffer.str();
}

// 2. Helper to extract text right after a specific class name
std::string extractTextForClass(const std::string& html, const std::string& className, size_t searchFrom = 0) {
    size_t classPos = html.find(className, searchFrom);
    if (classPos == std::string::npos) return "";

    size_t tagClosePos = html.find('>', classPos);
    if (tagClosePos == std::string::npos) return "";

    size_t nextTagPos = html.find('<', tagClosePos);
    if (nextTagPos == std::string::npos) return "";

    std::string result = html.substr(tagClosePos + 1, nextTagPos - (tagClosePos + 1));
    
    size_t first = result.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = result.find_last_not_of(" \t\r\n");
    return result.substr(first, (last - first + 1));
}

// 3. Convert "Jul 3" + "7:00 PM" -> "20260703T190000"
std::string formatAppleTimestamp(const std::string& dateStr, const std::string& timeStr) {
    const std::string months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    std::string monthNum = "01";
    for (int i = 0; i < 12; ++i) {
        if (dateStr.find(months[i]) != std::string::npos) {
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << (i + 1);
            monthNum = ss.str();
            break;
        }
    }

    std::string dayNum = "01";
    size_t spacePos = dateStr.find(' ');
    if (spacePos != std::string::npos && spacePos + 1 < dateStr.length()) {
        int dayInt = std::atoi(dateStr.substr(spacePos + 1).c_str());
        if (dayInt > 0 && dayInt <= 31) {
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << dayInt;
            dayNum = ss.str();
        }
    }

    int hours = 12;
    int minutes = 0;
    
    if (!timeStr.empty()) {
        hours = std::atoi(timeStr.c_str());
        size_t colonPos = timeStr.find(':');
        if (colonPos != std::string::npos && colonPos + 1 < timeStr.length()) {
            minutes = std::atoi(timeStr.substr(colonPos + 1).c_str());
        }

        bool isPM = (timeStr.find("PM") != std::string::npos || timeStr.find("pm") != std::string::npos);
        bool isAM = (timeStr.find("AM") != std::string::npos || timeStr.find("am") != std::string::npos);

        if (isPM && hours < 12) hours += 12;
        if (isAM && hours == 12) hours = 0;
    }

    std::stringstream timeFormatted;
    timeFormatted << std::setw(2) << std::setfill('0') << hours
                  << std::setw(2) << std::setfill('0') << minutes
                  << "00";

    return "2026" + monthNum + dayNum + "T" + timeFormatted.str();
}

// 4. NEW: Add 2 hours to start_time to make a realistic end_time
std::string calculateEndTime(const std::string& startTime, int durationHours = 2) {
    if (startTime.length() < 15) return startTime; // Fallback if format is wrong

    // Extract hour from "YYYYMMDDTHHMMSS" (characters at index 9 and 10)
    int hour = std::atoi(startTime.substr(9, 2).c_str());
    hour = (hour + durationHours) % 24; // Keep it within 0-23 hours

    std::stringstream newHour;
    newHour << std::setw(2) << std::setfill('0') << hour;

    std::string endTime = startTime;
    endTime.replace(9, 2, newHour.str());
    return endTime;
}

// 5. Parse your specific event cards from the HTML
std::vector<Event> parseEvents(const std::string& html) {
    std::vector<Event> events;
    size_t currentPos = 0;

    while ((currentPos = html.find("event-card-mobile", currentPos)) != std::string::npos) {
        Event e;
        
        e.title    = extractTextForClass(html, "title-calendar", currentPos);
        e.date_str = extractTextForClass(html, "date-calendar", currentPos);
        e.time_str = extractTextForClass(html, "time-calendar", currentPos);
        e.location = extractTextForClass(html, "location-calendar", currentPos);

        // Calculate both START and END time!
        e.start_time = formatAppleTimestamp(e.date_str, e.time_str);
        e.end_time   = calculateEndTime(e.start_time, 2); // Defaults to a 2-hour event

        if (!e.title.empty()) {
            events.push_back(e);
        }

        currentPos += 17; 
    }

    return events;
}

// 6. Generate the RFC 5545-compliant .ics file
void generateICS(const std::vector<Event>& events, const std::string& filename) {
    std::ofstream icsFile(filename);
    
    icsFile << "BEGIN:VCALENDAR\r\n";
    icsFile << "VERSION:2.0\r\n";
    icsFile << "PRODID:-//My C++ Scraper//EN\r\n";
    icsFile << "CALSCALE:GREGORIAN\r\n";

    for (size_t i = 0; i < events.size(); ++i) {
        icsFile << "BEGIN:VEVENT\r\n";
        icsFile << "UID:event-" << i << "@mycustomdomain.com\r\n";
        
        // Includes both START and END times now!
        icsFile << "DTSTART:" << events[i].start_time << "\r\n";
        icsFile << "DTEND:" << events[i].end_time << "\r\n";
        
        icsFile << "SUMMARY:" << events[i].title << "\r\n";
        icsFile << "LOCATION:" << events[i].location << "\r\n";
        icsFile << "DESCRIPTION:Date: " << events[i].date_str 
                << " | Time: " << events[i].time_str << "\r\n";
        
        icsFile << "END:VEVENT\r\n";
    }

    icsFile << "END:VCALENDAR\r\n";
    icsFile.close();
}

int main() {
    // Replace with your target website URL
    std::string targetURL = "https://www.shiftededit.com/events/july";
    
    std::cout << "Downloading HTML..." << std::endl;
    std::string html = fetchHTML(targetURL);
    
    std::cout << "Parsing events..." << std::endl;
    std::vector<Event> events = parseEvents(html);
    
    std::cout << "Found " << events.size() << " events!" << std::endl;
    for (const auto& e : events) {
        std::cout << " - " << e.title << " | Start: [" << e.start_time 
                  << "] -> End: [" << e.end_time << "]" << std::endl;
    }

    generateICS(events, "calendar.ics");
    std::cout << "Successfully generated calendar.ics!" << std::endl;
    return 0;
}