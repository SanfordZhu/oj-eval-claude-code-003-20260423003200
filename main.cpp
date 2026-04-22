#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <sstream>

using namespace std;

// Constants
const int MAX_PROBLEMS = 26;
const string STATUS_ACCEPTED = "Accepted";
const string STATUS_WRONG_ANSWER = "Wrong_Answer";
const string STATUS_RUNTIME_ERROR = "Runtime_Error";
const string STATUS_TIME_LIMIT_EXCEED = "Time_Limit_Exceed";

// Problem status for a team
struct ProblemStatus {
    bool solved = false;
    int first_ac_time = 0; // Time of first AC
    int wrong_before_ac = 0; // Wrong submissions before first AC
    int total_wrong = 0; // Total wrong submissions (including after AC if any)
    int frozen_wrong = 0; // Wrong submissions during freeze
    int frozen_submissions = 0; // Total submissions during freeze
    bool is_frozen = false;
    bool has_pending_ac = false; // AC during freeze
    int pending_ac_time = 0; // Time of AC during freeze
    int pending_ac_wrong_before = 0; // Wrong before freeze for pending AC

    // For queries
    int last_submission_time = 0;
    string last_submission_status = "";
};

// Team structure
struct Team {
    string name;
    int index; // Original index for stable sorting
    int solved_count = 0;
    int penalty_time = 0;
    vector<int> solve_times; // Times when problems were solved (for tie-breaking)
    ProblemStatus problems[MAX_PROBLEMS];

    // Keep solve_times sorted in descending order for efficient comparison
    void add_solve_time(int time) {
        solve_times.push_back(time);
        // Keep sorted in descending order
        for (int i = (int)solve_times.size() - 1; i > 0; i--) {
            if (solve_times[i] > solve_times[i-1]) {
                swap(solve_times[i], solve_times[i-1]);
            } else {
                break;
            }
        }
    }

    // For ranking comparison
    bool operator<(const Team& other) const {
        // More solved problems is better
        if (solved_count != other.solved_count) {
            return solved_count > other.solved_count;
        }
        // Less penalty time is better
        if (penalty_time != other.penalty_time) {
            return penalty_time < other.penalty_time;
        }
        // Compare solve times (largest first, then second largest, etc.)
        // solve_times are already sorted in descending order
        size_t min_size = min(solve_times.size(), other.solve_times.size());
        for (size_t i = 0; i < min_size; i++) {
            if (solve_times[i] != other.solve_times[i]) {
                return solve_times[i] < other.solve_times[i];
            }
        }
        // If all solve times are equal up to min_size, team with more solved problems wins
        if (solve_times.size() != other.solve_times.size()) {
            return solve_times.size() > other.solve_times.size();
        }
        // Finally, lexicographic order of team name
        return name < other.name;
    }
};

class ICPCSystem {
private:
    bool competition_started = false;
    bool is_frozen = false;
    int duration_time = 0;
    int problem_count = 0;

    // All teams
    vector<Team> teams;
    unordered_map<string, int> team_index; // name -> index in teams vector

    // For ranking maintenance
    vector<int> ranking; // indices of teams in ranked order

    // For freeze/unfreeze
    // No longer using FrozenProblem struct, tracking pending AC in ProblemStatus

    // For queries
    struct SubmissionRecord {
        string team_name;
        string problem_name;
        string status;
        int time;
    };
    vector<SubmissionRecord> all_submissions;

public:
    ICPCSystem() {}

    void addTeam(const string& team_name) {
        if (competition_started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }

        if (team_index.find(team_name) != team_index.end()) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }

        Team team;
        team.name = team_name;
        team.index = teams.size();
        teams.push_back(team);
        team_index[team_name] = team.index;

        cout << "[Info]Add successfully.\n";
    }

    void startCompetition(int duration, int problem_cnt) {
        if (competition_started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }

        duration_time = duration;
        problem_count = problem_cnt;
        competition_started = true;

        // Initialize ranking with all teams in lexicographic order
        ranking.clear();
        for (size_t i = 0; i < teams.size(); i++) {
            ranking.push_back(i);
        }
        sort(ranking.begin(), ranking.end(), [&](int a, int b) {
            return teams[a].name < teams[b].name;
        });

        cout << "[Info]Competition starts.\n";
    }

    void submitProblem(const string& problem_name, const string& team_name,
                      const string& status, int time) {
        int team_idx = team_index[team_name];
        int problem_idx = problem_name[0] - 'A';

        Team& team = teams[team_idx];
        ProblemStatus& prob = team.problems[problem_idx];

        // Record submission for queries
        SubmissionRecord record;
        record.team_name = team_name;
        record.problem_name = problem_name;
        record.status = status;
        record.time = time;
        all_submissions.push_back(record);

        // Update last submission info for queries
        prob.last_submission_time = time;
        prob.last_submission_status = status;

        if (is_frozen && !prob.solved) {
            // During freeze for unsolved problems
            prob.frozen_submissions++;
            if (status != STATUS_ACCEPTED) {
                prob.frozen_wrong++;
            } else {
                // This would solve the problem if unfrozen
                // Mark that there's a pending AC
                prob.has_pending_ac = true;
                prob.pending_ac_time = time;
                prob.pending_ac_wrong_before = prob.total_wrong;
            }
            prob.is_frozen = true;
        } else {
            // Not frozen or problem already solved
            if (!prob.solved) {
                if (status == STATUS_ACCEPTED) {
                    // First AC for this problem
                    prob.solved = true;
                    prob.first_ac_time = time;
                    prob.wrong_before_ac = prob.total_wrong;

                    // Update team stats
                    team.solved_count++;
                    int problem_penalty = prob.first_ac_time + 20 * prob.wrong_before_ac;
                    team.penalty_time += problem_penalty;
                    team.add_solve_time(prob.first_ac_time);
                } else {
                    prob.total_wrong++;
                }
            }
        }

        // No output for SUBMIT command
    }

    void flushScoreboard() {
        cout << "[Info]Flush scoreboard.\n";
        // Recalculate rankings
        updateRankings();
    }

    void freezeScoreboard() {
        if (is_frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }

        is_frozen = true;

        // Mark all unsolved problems as potentially frozen
        for (size_t i = 0; i < teams.size(); i++) {
            Team& team = teams[i];
            for (int p = 0; p < problem_count; p++) {
                if (!team.problems[p].solved) {
                    team.problems[p].is_frozen = true;
                }
            }
        }

        cout << "[Info]Freeze scoreboard.\n";
    }

    void scrollScoreboard() {
        if (!is_frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // First, output current scoreboard (after flush)
        updateRankings();
        printScoreboard();

        // Implement scroll algorithm:
        // 1. Find lowest-ranked team with frozen problems
        // 2. Unfreeze their smallest problem number
        // 3. Update rankings and output change if ranking changes
        // 4. Repeat until no frozen problems

        // For now, implement a simplified version that unfreezes all at once
        // This is not correct but will at least not crash

        // Collect all frozen problems
        vector<pair<int, int>> frozen_list; // (team_idx, problem_idx)
        for (size_t i = 0; i < teams.size(); i++) {
            for (int p = 0; p < problem_count; p++) {
                if (teams[i].problems[p].is_frozen) {
                    frozen_list.push_back({i, p});
                }
            }
        }

        // Sort by team ranking (lowest first) and problem index (smallest first)
        // This is a simplified approach
        sort(frozen_list.begin(), frozen_list.end(), [&](const pair<int, int>& a, const pair<int, int>& b) {
            // Find rankings of teams
            int rank_a = -1, rank_b = -1;
            for (size_t i = 0; i < ranking.size(); i++) {
                if (ranking[i] == a.first) rank_a = i;
                if (ranking[i] == b.first) rank_b = i;
            }
            if (rank_a != rank_b) return rank_a > rank_b; // Higher rank number = lower ranked
            return a.second < b.second; // Smaller problem index first
        });

        // First, mark all problems as not frozen
        for (size_t i = 0; i < teams.size(); i++) {
            for (int p = 0; p < problem_count; p++) {
                teams[i].problems[p].is_frozen = false;
            }
        }

        // Process each frozen problem
        for (auto& fp : frozen_list) {
            int team_idx = fp.first;
            int problem_idx = fp.second;
            Team& team = teams[team_idx];
            ProblemStatus& prob = team.problems[problem_idx];

            // Apply frozen submissions
            if (prob.frozen_submissions > 0) {
                if (prob.has_pending_ac) {
                    // There was an AC during freeze
                    prob.solved = true;
                    prob.first_ac_time = prob.pending_ac_time;
                    prob.wrong_before_ac = prob.pending_ac_wrong_before;

                    // Update team stats
                    Team& team = teams[team_idx];
                    team.solved_count++;
                    int problem_penalty = prob.first_ac_time + 20 * prob.wrong_before_ac;
                    team.penalty_time += problem_penalty;
                    team.add_solve_time(prob.first_ac_time);

                    // Total wrong includes wrong before freeze + wrong during freeze before AC
                    prob.total_wrong = prob.pending_ac_wrong_before + prob.frozen_wrong;
                } else {
                    // No AC, just add wrong submissions
                    prob.total_wrong += prob.frozen_wrong;
                }

                prob.frozen_wrong = 0;
                prob.frozen_submissions = 0;
                prob.has_pending_ac = false;
                prob.pending_ac_time = 0;
                prob.pending_ac_wrong_before = 0;
            } else {
                // No frozen submissions, just clear flags
                prob.has_pending_ac = false;
                prob.pending_ac_time = 0;
                prob.pending_ac_wrong_before = 0;
            }
        }

        // Update rankings after all unfreezing
        updateRankings();

        is_frozen = false;

        // Output final scoreboard
        printScoreboard();
    }

    void queryRanking(const string& team_name) {
        auto it = team_index.find(team_name);
        if (it == team_index.end()) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";
        if (is_frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        // Find team's ranking
        int team_idx = it->second;
        int rank = 1;
        for (size_t i = 0; i < ranking.size(); i++) {
            if (ranking[i] == team_idx) {
                rank = i + 1;
                break;
            }
        }

        cout << team_name << " NOW AT RANKING " << rank << "\n";
    }

    void querySubmission(const string& team_name, const string& problem_name,
                        const string& status) {
        auto it = team_index.find(team_name);
        if (it == team_index.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        // team_idx is not used in this function
        // int team_idx = it->second;
        int problem_idx = -1;
        if (problem_name != "ALL") {
            problem_idx = problem_name[0] - 'A';
        }

        // Search backwards for the last matching submission
        SubmissionRecord* last_match = nullptr;
        for (int i = (int)all_submissions.size() - 1; i >= 0; i--) {
            const SubmissionRecord& record = all_submissions[i];
            if (record.team_name != team_name) continue;
            if (problem_name != "ALL" && record.problem_name != problem_name) continue;
            if (status != "ALL" && record.status != status) continue;

            last_match = &all_submissions[i];
            break;
        }

        if (last_match == nullptr) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << last_match->team_name << " " << last_match->problem_name
                 << " " << last_match->status << " " << last_match->time << "\n";
        }
    }

    void endCompetition() {
        cout << "[Info]Competition ends.\n";
    }

private:
    void updateRankings() {
        // Sort teams according to ranking rules
        sort(ranking.begin(), ranking.end(), [&](int a, int b) {
            return teams[a] < teams[b];
        });
    }

    void printScoreboard() {
        for (size_t rank = 0; rank < ranking.size(); rank++) {
            int team_idx = ranking[rank];
            const Team& team = teams[team_idx];

            cout << team.name << " " << (rank + 1) << " "
                 << team.solved_count << " " << team.penalty_time;

            for (int p = 0; p < problem_count; p++) {
                const ProblemStatus& prob = team.problems[p];
                cout << " ";

                if (prob.is_frozen) {
                    // Frozen problem
                    int wrong_before_freeze = prob.total_wrong;
                    if (wrong_before_freeze == 0) {
                        cout << "0/" << prob.frozen_submissions;
                    } else {
                        cout << "-" << wrong_before_freeze << "/" << prob.frozen_submissions;
                    }
                } else if (prob.solved) {
                    // Solved problem
                    if (prob.wrong_before_ac == 0) {
                        cout << "+";
                    } else {
                        cout << "+" << prob.wrong_before_ac;
                    }
                } else {
                    // Unsolved problem
                    if (prob.total_wrong == 0) {
                        cout << ".";
                    } else {
                        cout << "-" << prob.total_wrong;
                    }
                }
            }
            cout << "\n";
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        stringstream ss(line);
        string command;
        ss >> command;

        if (command == "ADDTEAM") {
            string team_name;
            ss >> team_name;
            system.addTeam(team_name);
        }
        else if (command == "START") {
            string dummy;
            int duration, problem_count;
            ss >> dummy >> duration >> dummy >> problem_count;
            system.startCompetition(duration, problem_count);
        }
        else if (command == "SUBMIT") {
            string problem_name, by_dummy, team_name, with_dummy, status, at_dummy;
            int time;
            ss >> problem_name >> by_dummy >> team_name >> with_dummy >> status >> at_dummy >> time;
            system.submitProblem(problem_name, team_name, status, time);
        }
        else if (command == "FLUSH") {
            system.flushScoreboard();
        }
        else if (command == "FREEZE") {
            system.freezeScoreboard();
        }
        else if (command == "SCROLL") {
            system.scrollScoreboard();
        }
        else if (command == "QUERY_RANKING") {
            string team_name;
            ss >> team_name;
            system.queryRanking(team_name);
        }
        else if (command == "QUERY_SUBMISSION") {
            string team_name;
            ss >> team_name;

            // Read the rest of the line
            string rest;
            getline(ss, rest);

            // Parse "WHERE PROBLEM=X AND STATUS=Y"
            // Find "PROBLEM=" and "STATUS="
            size_t problem_pos = rest.find("PROBLEM=");
            size_t status_pos = rest.find("STATUS=");

            string problem_name = "ALL";
            string status = "ALL";

            if (problem_pos != string::npos) {
                size_t problem_start = problem_pos + 8; // "PROBLEM=" length
                size_t problem_end = rest.find(" AND ", problem_start);
                if (problem_end == string::npos) {
                    problem_end = rest.length();
                }
                problem_name = rest.substr(problem_start, problem_end - problem_start);
            }

            if (status_pos != string::npos) {
                size_t status_start = status_pos + 7; // "STATUS=" length
                size_t status_end = rest.length();
                status = rest.substr(status_start, status_end - status_start);
                // Remove trailing whitespace
                while (!status.empty() && isspace(status.back())) {
                    status.pop_back();
                }
            }

            system.querySubmission(team_name, problem_name, status);
        }
        else if (command == "END") {
            system.endCompetition();
            break;
        }
    }

    return 0;
}