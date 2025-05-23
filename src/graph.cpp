#include "graph.hpp"

void Graph::addCity(const string& name) {
    if (!containsCity(name)) {
        adj[name] = {};
        numberOfCities++;
    }
}

int Graph::getnumberOfCities()
{
    return numberOfCities;
}

vector<string> Graph::getAllCities()
{
    vector<string>res;
    for(auto &[city,_]:adj)
    {
        res.push_back(city);
    }
    return res;
}

void Graph::addEdge(const string& src, const string& dest, double distance, double time) {
    if (src == dest) return;
    if (distance < 0) distance *= -1;
    if (time < 0) time *= -1;

    // If the edge already exists, it will be updated.
    adj[src][dest] = {distance, time};
    adj[dest][src] = {distance, time};
}

void Graph::deleteCity(const string& name) {
    if (!containsCity(name)) return;
    for (auto& [city, neighbors] : adj) {
        neighbors.erase(name);
    }
    adj.erase(name);
    numberOfCities--;
}

void Graph::deleteEdge(const string& src, const string& dest) {
    if (!containsEdge(src, dest)) return;
    adj[src].erase(dest);
    adj[dest].erase(src);
}

bool Graph::containsCity(const string& name){
    return(adj.find(name) != adj.end());
}

bool Graph::containsEdge(const string& city1, const string& city2) {
    if (containsCity(city1) && adj[city1].find(city2) != adj[city1].end()) {
        return true;
    }

    if (containsCity(city2) && adj[city2].find(city1) != adj[city2].end()) {
        return true;
    }

    return false;
}


vector<string> Graph::BFS(const string& start) {
    vector<string> result;

    if (!containsCity(start)) return result;

    unordered_set<string> visited;
    queue<string> q;

    q.push(start);
    visited.insert(start);

    while (!q.empty()) {
        string city = q.front();
        q.pop();
        result.push_back(city);

        for (const auto& [neighbor, _ ] : adj[city]) { //structured bindings inside range-based for loop
            if (visited.find(neighbor) == visited.end()) { //couldn't find
                visited.insert(neighbor);
                q.push(neighbor);
            }
        }
    }

    return result;
}

vector<string> Graph::DFS(const string& start) {
    vector<string> result;

    if (!containsCity(start)) return result;

    unordered_set<string> visited;
    stack<string> st;

    st.push(start);

    while (!st.empty()) {
        string city = st.top();
        st.pop();

        if (visited.find(city) == visited.end()) {
            visited.insert(city);
            result.push_back(city);

            for (const auto& [neighbor, _] : adj[city]) {
                if (visited.find(neighbor) == visited.end()) {
                    st.push(neighbor);
                }
            }

        }
    }

    return result;
}

Graph::PathResult Graph::DijkstraDistance(const string& start, const string& destination) {
    PathResult newResult;

    if (!containsCity(start) || !containsCity(destination)) {
        return newResult;
    }

    unordered_map<string, double> minDistance;
    unordered_map<string, string> previous;
    priority_queue<pair<double, string>,
                        vector<pair<double, string>>,
                        greater<>> pq;

    for (const auto& cityPair : adj) {
        minDistance[cityPair.first] = numeric_limits<double>::infinity();
    }

    minDistance[start] = 0.0;
    pq.push({0.0, start});

    while (!pq.empty()) {
        auto [distSoFar, city] = pq.top();
        pq.pop();

        if (distSoFar > minDistance[city]) continue;
        if (city == destination) break;

        for (const auto& [neighbor, data] : adj[city]) {
            double edgeDist = data.first;
            double newDist = distSoFar + edgeDist;

            if (newDist < minDistance[neighbor]) {
                minDistance[neighbor] = newDist;
                previous[neighbor] = city;
                pq.push({newDist, neighbor});
            }
        }
    }

    if (minDistance[destination] == numeric_limits<double>::infinity()) {
        return newResult;
    }

    // Reconstruct path
    for (string cur = destination; ; cur = previous[cur]) {
        newResult.path.push_back(cur);
        if (cur == start) break;
    }
    reverse(newResult.path.begin(), newResult.path.end());
    newResult.distanceOrTime = minDistance[destination];

    return newResult;
}


Graph::PathResult Graph::DijkstraTime(const string& start, const string& destination) {
    PathResult newResult;

    if (!containsCity(start) || !containsCity(destination)) {
        return newResult;
    }


    unordered_map<string, double> minTime;
    unordered_map<string, string> previous;
    priority_queue<pair<double, string>,
                        vector<pair<double, string>>,
                        greater<>> pq;


    for (auto& cityPair : adj) {
        minTime[cityPair.first] = numeric_limits<double>::infinity();
    }
    minTime[start] = 0.0;
    pq.push({0.0, start});


    while (!pq.empty()) {
        auto [timeSoFar, city] = pq.top();
        pq.pop();
        if (timeSoFar > minTime[city]) continue;
        if (city == destination) break;

        for (auto& [neighbor, data] : adj[city]) {
            double edgeTime = data.second;
            double newTime = timeSoFar + edgeTime;
            if (newTime < minTime[neighbor]) {
                minTime[neighbor] = newTime;
                previous[neighbor] = city;
                pq.push({newTime, neighbor});
            }
        }
    }


    if (minTime[destination] == numeric_limits<double>::infinity()) {
        return newResult;
    }


    for (string cur = destination; ; cur = previous[cur]) {
        newResult.path.push_back(cur);
        if (cur == start) break;
    }
    reverse(newResult.path.begin(), newResult.path.end());
    newResult.distanceOrTime = minTime[destination];

    return newResult;
}
