#include "Game.hpp"

#include "Connection.hpp"

#include "data_path.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include <fstream>
#include <sstream>

#include <glm/gtx/norm.hpp>

void Player::Controls::send_controls_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 6;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b) {
		if (b.downs & 0x80) {
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t( (b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f) ) );
	};

	send_button(left);
	send_button(right);
	send_button(up);
	send_button(down);
	send_button(shift);
	send_button(ret);
}

bool Player::Controls::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	//expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size != 6) throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 6!");
	
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	auto recv_button = [](uint8_t byte, Button *button) {
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255) {
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	recv_button(recv_buffer[4+0], &left);
	recv_button(recv_buffer[4+1], &right);
	recv_button(recv_buffer[4+2], &up);
	recv_button(recv_buffer[4+3], &down);
	recv_button(recv_buffer[4+4], &shift);
	recv_button(recv_buffer[4+5], &ret);

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}

int Player::score() const {
	return 2 * fill_correct + x_correct;
}

//-----------------------------------------

void Game::render_numbers(uint32_t w, uint32_t h, std::vector<std::vector<uint32_t>> data) {
	clues.width = w;
	clues.height = h;
	clues.by_row.clear();
	clues.by_col.clear();
	assert(data.size() >= h);

	for (std::vector<uint32_t> row : data) {
		std::vector<uint32_t> amts;
		uint32_t run = 0;
		for (uint32_t cell: row) {
			if (cell) run++;
			else {
				if (run) amts.push_back(run);
				run = 0;
			}
		}
		if (run || !amts.size()) amts.push_back(run);
		clues.by_row.push_back(amts);
	}

	for (uint32_t x = 0; x < w; x++) {
		std::vector<uint32_t> amts;
		uint32_t run = 0;
		for (uint32_t y = 0; y < h; y++) {
			if (data[y][x]) run++;
			else {
				if (run) amts.push_back(run);
				run = 0;
			}
		}
		if (run || !amts.size()) amts.push_back(run);
		clues.by_col.push_back(amts);
	}
}

void Game::clear_grid() {
	grid.progress.clear();
	grid.solution.clear();
}

void Game::make_grid_random() {
	uint32_t w = dim(mt_grid);
	uint32_t h = dim(mt_grid);
	width = w;
	height = h;
	for (uint32_t j = 0; j < height; j++) {
		std::vector<uint32_t> next_row;
		std::vector<int> blank_row;
		for (uint32_t i = 0; i < width; i++) {
			next_row.push_back(mt_grid() % 2);
			blank_row.push_back(0);
		}
		assert(next_row.size() == width);
		assert(blank_row.size() == width);
		grid.solution.push_back(next_row);
		grid.progress.push_back(blank_row);
	}
	assert(grid.solution.size() == height);
	assert(grid.progress.size() == height);
	render_numbers(width, height, grid.solution);
}

void Game::make_grid_file() {
	std::uniform_int_distribution<> custom_picker(0, (int)customs.size() - 1);
	std::string name = customs[custom_picker(mt_grid)];

	uint32_t lnum = 0;
    std::ifstream infile(data_path("custom/" + name + ".txt"));
	std::string line;

	uint32_t w, h;
	while (std::getline(infile, line)) {
		if (lnum == 0) {
			// width and height
			std::istringstream iss(line);
			iss >> h >> w;
			height = h;
			width = w;
		}
		else {
			std::vector<uint32_t> next_row;
			std::vector<int> blank_row;
			for (uint32_t i = 0; i < width; i++) {
				next_row.push_back((line[i] == 'o' ? 1 : 0));
				blank_row.push_back(0);
			}
			assert(next_row.size() == width);
			assert(blank_row.size() == width);
			grid.solution.push_back(next_row);
			grid.progress.push_back(blank_row);
		}
		lnum++;
    }
	assert(grid.solution.size() == height);
	assert(grid.progress.size() == height);
	render_numbers(width, height, grid.solution);
}

void Game::reset_positions() {
	// reset player positions
	for (auto &player : players) {
		// top-left of the arena
		player.position = glm::vec2{ArenaMin.x + cellSize / 2.0f, ArenaMax.y - cellSize / 2.0f};
		player.grid_pos = glm::uvec2(0, 0);
	}
}

void Game::reset_routine() {
	clear_grid();
	std::uniform_int_distribution<int> biased_coin(0, 4);
	if (biased_coin(mt_grid) < 2) make_grid_file();
	else make_grid_random();

	ArenaMin = glm::vec2(-(float)width / 2.0f, -(float)height / 2.0f) * cellSize;
	ArenaMax = glm::vec2( (float)width / 2.0f,  (float)height / 2.0f) * cellSize;
	reset_positions();
	paused = false;
}

Game::Game() : mt(0x15466789) {
	std::random_device rd;
	mt_grid = std::mt19937(rd());
	dim = std::uniform_int_distribution<>(5, 7);
	
	reset_routine();
}

Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	// top-left of the arena
	player.position = glm::vec2{ArenaMin.x + cellSize / 2.0f, ArenaMax.y - cellSize / 2.0f};
	player.grid_pos = glm::uvec2(0, 0);

	do {
		player.color.r = mt() / float(mt.max());
		player.color.g = mt() / float(mt.max());
		player.color.b = mt() / float(mt.max());
	} while (player.color == glm::vec3(0.0f));
	player.color = glm::normalize(player.color);

	player.name = "Player " + std::to_string(next_player_number);
	player.id = next_player_number;
	next_player_number++;

	colormap[player.id] = player.color;

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

bool Game::completed_grid() {
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			if ((grid.solution[y][x] == 0) ^ (grid.progress[y][x] <= 0)) return false;
		}
	}
	return true;
}
void Game::offscreen_players() {
	for (auto &player : players) {
		// throw them in the top-right somewhere where they won't come back until unpaused
		player.position = ArenaMax * 10.0f;
		player.grid_pos = glm::uvec2(0, 0);
	}
}

void Game::clear_xs() {
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			grid.progress[y][x] = std::max(grid.progress[y][x], 0);
		}
	}
}

void Game::update(float elapsed) {
	// cooldown timers
	if (paused) {
		// do nothing
		global_cooldown -= elapsed;
		if (global_cooldown <= 0.0f) {
			// NOW we resume
			reset_routine();
		} else return;
	}

	// position responding
	for (auto &p : players) {
		if (p.player_cooldown > 0.0f) p.player_cooldown -= elapsed;

		if (p.controls.left.pressed) {
			if (p.grid_pos.x > 0) {
				p.position.x -= cellSize;
				p.grid_pos.x -= 1;
			}
		}
		if (p.controls.right.pressed) {
			if (p.grid_pos.x < width - 1) {
				p.position.x += cellSize;
				p.grid_pos.x += 1;
			}
		}
		if (p.controls.up.pressed) {
			if (p.grid_pos.y > 0) {
				p.position.y += cellSize;
				p.grid_pos.y -= 1;
			}
		}
		if (p.controls.down.pressed) {
			if (p.grid_pos.y < height - 1) {
				p.position.y -= cellSize;
				p.grid_pos.y += 1;
			}
		}
		if (p.controls.shift.pressed) p.fill_mode = !p.fill_mode;
		if (p.controls.ret.pressed) {
			if (p.player_cooldown > 0.0f) continue; // player is stuck

			// std::cout << p.grid_pos.x << ", " << p.grid_pos.y << std::endl;
			if (grid.progress[p.grid_pos.y][p.grid_pos.x] != 0) continue; // already completed
			if (p.fill_mode && grid.solution[p.grid_pos.y][p.grid_pos.x]) {
				p.fill_correct++;
				grid.progress[p.grid_pos.y][p.grid_pos.x] = p.id;
			}
			else if (p.fill_mode && !grid.solution[p.grid_pos.y][p.grid_pos.x]) {
				p.fill_incorrect++;
				p.player_cooldown = wrong_cooldown;
			}
			else if (!p.fill_mode && grid.solution[p.grid_pos.y][p.grid_pos.x]) {
				p.x_incorrect++;
				p.player_cooldown = wrong_cooldown;
			}
			else if (!p.fill_mode && !grid.solution[p.grid_pos.y][p.grid_pos.x]) {
				p.x_correct++;
				grid.progress[p.grid_pos.y][p.grid_pos.x] = -p.id;
			}
			else {} // shouldn't happen
		}

		//reset 'downs' since controls have been handled:
		p.controls.left.downs = 0;
		p.controls.right.downs = 0;
		p.controls.up.downs = 0;
		p.controls.down.downs = 0;
		p.controls.shift.downs = 0;
		p.controls.ret.downs = 0;
	}

	// //collision resolution:
	for (auto &p1 : players) {
	// 	//player/player collisions:
	// 	for (auto &p2 : players) {
	// 		if (&p1 == &p2) break;
	// 		glm::vec2 p12 = p2.position - p1.position;
	// 		float len2 = glm::length2(p12);
	// 		if (len2 > (2.0f * PlayerRadius) * (2.0f * PlayerRadius)) continue;
	// 		if (len2 == 0.0f) continue;
	// 		glm::vec2 dir = p12 / std::sqrt(len2);
	// 		//mirror velocity to be in separating direction:
	// 		glm::vec2 v12 = p2.velocity - p1.velocity;
	// 		glm::vec2 delta_v12 = dir * glm::max(0.0f, -1.75f * glm::dot(dir, v12));
	// 		p2.velocity += 0.5f * delta_v12;
	// 		p1.velocity -= 0.5f * delta_v12;
	// 	}
	//player/arena collisions:
		if (p1.position.x < ArenaMin.x + PlayerRadius) {
			p1.position.x = ArenaMin.x + PlayerRadius;
		}
		if (p1.position.x > ArenaMax.x - PlayerRadius) {
			p1.position.x = ArenaMax.x - PlayerRadius;
		}
		if (p1.position.y < ArenaMin.y + PlayerRadius) {
			p1.position.y = ArenaMin.y + PlayerRadius;
		}
		if (p1.position.y > ArenaMax.y - PlayerRadius) {
			p1.position.y = ArenaMax.y - PlayerRadius;
		}
	}
	if (completed_grid()) {
		// prepare new game
		paused = true;
		global_cooldown = finished_cooldown;
		clear_xs();
		offscreen_players();
	}
}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	//send player info helper:
	auto send_player = [&](Player const &player) {
		connection.send(player.position);
		connection.send(player.grid_pos);
		connection.send(player.fill_mode);
		connection.send(player.id);
		connection.send(player.color);

		connection.send(player.fill_correct);
		connection.send(player.fill_incorrect);
		connection.send(player.x_correct);
		connection.send(player.x_incorrect);

		connection.send(player.player_cooldown);
	
		//NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
		//effectively: truncates player name to 255 chars
		uint8_t len = uint8_t(std::min< size_t >(255, player.name.size()));
		connection.send(len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		if (&player == connection_player) continue;
		send_player(player);
	}

	// colormap:
	connection.send(colormap.size());
	for (std::pair<uint32_t, glm::vec3> p : colormap) {
		connection.send(p.first);
		connection.send(p.second);
	};

	auto send_vec_vec = [&](std::vector<std::vector<int>> data) {
		connection.send(data.size());
		for (std::vector<int> v : data) {
			connection.send(v.size());
			for (int i : v) {
				connection.send(i);
			}
		}
	};
	auto send_vec_uvec = [&](std::vector<std::vector<uint32_t>> data) {
		connection.send(data.size());
		for (std::vector<uint32_t> v : data) {
			connection.send(v.size());
			for (uint32_t i : v) {
				connection.send(i);
			}
		}
	};

	// puzzle information
	connection.send(width);
	connection.send(height);
	connection.send(ArenaMin);
	connection.send(ArenaMax);

	connection.send(clues.height);
	connection.send(clues.width);
	send_vec_uvec(clues.by_row);
	send_vec_uvec(clues.by_col);
	send_vec_vec(grid.progress);

	connection.send(global_cooldown);

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//copy bytes from buffer and advance position:
	auto read = [&](auto *val) {
		if (at + sizeof(*val) > size) {
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read(&player.position);
		read(&player.grid_pos);
		read(&player.fill_mode);
		read(&player.id);
		read(&player.color);

		read(&player.fill_correct);
		read(&player.fill_incorrect);
		read(&player.x_correct);
		read(&player.x_incorrect);

		read(&player.player_cooldown);

		uint8_t name_len;
		read(&name_len);
		//n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
		player.name = "";
		for (uint8_t n = 0; n < name_len; ++n) {
			char c;
			read(&c);
			player.name += c;
		}
	}

	// colormap:
	colormap.clear();
	size_t colormap_size;
	read(&colormap_size);
	for (uint32_t i = 0; i < colormap_size; i++) {
		uint32_t id;
		glm::vec3 color;
		read(&id);
		read(&color);
		colormap[id] = color;
	};

	read(&width);
	read(&height);
	read(&ArenaMin);
	read(&ArenaMax);

	auto read_vec_vec = [&](std::vector<std::vector<int>> &target) {
		target.clear();
		size_t outer_size;
		read(&outer_size);
		for (size_t i = 0; i < outer_size; i++) {
			std::vector<int> next;
			size_t inner_size;
			read(&inner_size);
			for (size_t j = 0; j < inner_size; j++) {
				next.emplace_back();
				read(&next.back());
			}
			target.push_back(next);
		}
	};
	auto read_vec_uvec = [&](std::vector<std::vector<uint32_t>> &target) {
		target.clear();
		size_t outer_size;
		read(&outer_size);
		for (size_t i = 0; i < outer_size; i++) {
			std::vector<uint32_t> next;
			size_t inner_size;
			read(&inner_size);
			for (size_t j = 0; j < inner_size; j++) {
				next.emplace_back();
				read(&next.back());
			}
			target.push_back(next);
		}
	};
	read(&clues.height);
	read(&clues.width);
	read_vec_uvec(clues.by_row);
	read_vec_uvec(clues.by_col);
	read_vec_vec(grid.progress);

	read(&global_cooldown);

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
