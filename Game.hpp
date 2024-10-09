#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <random>
#include <unordered_map>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	C2S_Controls = 1,
	S2C_State = 's',
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		Button left, right, up, down, shift, ret;

		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	//player state (sent from server):
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	glm::uvec2 grid_pos = glm::uvec2(0, 0);
	// glm::vec2 velocity = glm::vec2(0.0f, 0.0f);

	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	std::string name = "";
	bool fill_mode = true;

	uint32_t id;

	uint32_t fill_correct, fill_incorrect, x_correct, x_incorrect = 0;

	float player_cooldown = 0.0f;

	int score() const;
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	std::mt19937 mt; // used for spawning players
	std::mt19937 mt_grid; // grid spawner
	std::uniform_int_distribution<> dim;
	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 10.0f;

	inline static uint32_t width = 20;
	inline static uint32_t height = 15;
	struct Clues {
		uint32_t width;
		uint32_t height;
		std::vector<std::vector<uint32_t>> by_row;
		std::vector<std::vector<uint32_t>> by_col;
	} clues;
	struct Grid {
		std::vector<std::vector<uint32_t>> solution;
		std::vector<std::vector<int>> progress;
	} grid;

	inline static std::vector<std::string> customs{"flower", "snowglobe"};

	void clear_grid();
	void make_grid_random();
	void make_grid_file();
	void reset_routine();

	void reset_positions();
	void render_numbers(uint32_t w, uint32_t h, std::vector<std::vector<uint32_t>> data);
	bool completed_grid();
	
	void clear_xs();
	void offscreen_players();

	std::unordered_map<uint32_t, glm::vec3> colormap;

	//arena size:
	inline static constexpr float cellSize = 0.1f;
	inline static glm::vec2 ArenaMin = glm::vec2(-(float)width / 2.0f, -(float)height / 2.0f) * cellSize;
	inline static glm::vec2 ArenaMax = glm::vec2( (float)width / 2.0f,  (float)height / 2.0f) * cellSize;

	//player constants:
	inline static constexpr float PlayerRadius = cellSize / 2;
	inline static constexpr float PlayerSpeed = 2.0f;
	inline static constexpr float PlayerAccelHalflife = 0.25f;

	inline static constexpr float wrong_cooldown = 4.0f;
	inline static constexpr float finished_cooldown = 3.0f;

	inline static float global_cooldown = 0.0f;
	inline static bool paused = false;
	

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;
};
