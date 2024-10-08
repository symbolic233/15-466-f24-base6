#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

// print debuggers
void PlayMode::print_grid() {
	for (uint32_t j = 0; j < Game::height; j++) {
		for (uint32_t i = 0; i < Game::width; i++) {
			std::cout << game.grid.solution[j][i];
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}
void PlayMode::print_clues() {
	std::cout << "Row clues:" << std::endl;
	for (std::vector<uint32_t> row_clue : game.clues.by_row) {
		for (uint32_t c : row_clue) {
			std::cout << c;
		}
		std::cout << std::endl;
	}
	std::cout << "Column clues:" << std::endl;
	for (std::vector<uint32_t> col_clue : game.clues.by_col) {
		for (uint32_t c : col_clue) {
			std::cout << c;
		}
		std::cout << std::endl;
	}
}

PlayMode::PlayMode(Client &client_) : client(client_) {
	// print_grid();
	// print_clues();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.keysym.sym == SDLK_a) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_LSHIFT || evt.key.keysym.sym == SDLK_RSHIFT) {
			controls.shift.downs += 1;
			controls.shift.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
			controls.ret.downs += 1;
			controls.ret.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_LSHIFT || evt.key.keysym.sym == SDLK_RSHIFT) {
			controls.shift.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
			controls.ret.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.shift.downs = 0;
	controls.ret.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();
	static std::array< glm::vec2, 4 > const square = [](){
		std::array< glm::vec2, 4 > ret;
		ret[0] = glm::vec2(1.0f, 1.0f);
		ret[1] = glm::vec2(-1.0f, 1.0f);
		ret[2] = glm::vec2(-1.0f, -1.0f);
		ret[3] = glm::vec2(1.0f, -1.0f);
		return ret;
	}();
	static std::array< glm::vec2, 7 > const cross = [](){
		std::array< glm::vec2, 7 > ret;
		ret[0] = glm::vec2(0.0f, 0.0f);
		ret[1] = glm::vec2(1.0f, 1.0f);
		ret[2] = glm::vec2(-1.0f, -1.0f);
		ret[3] = glm::vec2(0.0f, 0.0f);
		ret[4] = glm::vec2(1.0f, -1.0f);
		ret[5] = glm::vec2(-1.0f, 1.0f);
		ret[6] = glm::vec2(0.0f, 0.0f);
		return ret;
	}();

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		1.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		1.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	{
		DrawLines lines(world_to_clip);

		//helper:
		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = (1.0f / scale) / drawable_size.y;
			lines.draw_text(text,
				glm::vec3(at.x + ofs, at.y + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0xff, 0xff, 0xff));

		// draw clues
		glm::vec2 row_cell_pos(Game::ArenaMin.x - Game::cellSize, Game::ArenaMax.y - Game::cellSize);
		for (std::vector<uint32_t> row_clue : game.clues.by_row) {
			for (auto it = row_clue.rbegin(); it != row_clue.rend(); it++) {
				draw_text(row_cell_pos, std::to_string(*it), 0.09f);
				row_cell_pos.x -= Game::cellSize;
			}
			row_cell_pos.x = Game::ArenaMin.x - Game::cellSize;
			row_cell_pos.y -= Game::cellSize;
		}
		glm::vec2 col_cell_pos(Game::ArenaMin.x + 0.04f, Game::ArenaMax.y + Game::cellSize / 2.0f);
		for (std::vector<uint32_t> col_clue : game.clues.by_col) {
			for (auto it = col_clue.rbegin(); it != col_clue.rend(); it++) {
				draw_text(col_cell_pos, std::to_string(*it), 0.09f);
				col_cell_pos.y += Game::cellSize;
			}
			col_cell_pos.x += Game::cellSize;
			col_cell_pos.y = Game::ArenaMax.y + Game::cellSize / 2.0f;
		}

		auto draw_shape = [&](glm::vec2 pos, float scale, auto &shape, glm::u8vec4 color) {
			for (uint32_t a = 0; a < shape.size(); ++a) {
				lines.draw(
					glm::vec3(pos + Game::PlayerRadius * scale * shape[a], 0.0f),
					glm::vec3(pos + Game::PlayerRadius * scale * shape[(a+1)%shape.size()], 0.0f),
					color
				);
			}
		};

		// draw in grid
		float gscale = 0.75f;
		// std::cout << "trying to draw grid" << std::endl;
		for (uint32_t y = 0; y < Game::height; y++) {
			for (uint32_t x = 0; x < Game::width; x++) {
				if (game.grid.progress[y][x] == 0) continue;
				int key = game.grid.progress[y][x];
				auto colorcheck = game.colormap.find(std::abs(key));
				glm::vec3 base_color = (colorcheck != game.colormap.end()) ? colorcheck->second : glm::vec3{1.0f};
				glm::u8vec4 this_color{game.colormap[std::abs(key)] * 255.0f, 0xff};
				glm::vec2 cellPos{Game::ArenaMin.x + Game::cellSize / 2.0f, Game::ArenaMax.y - Game::cellSize / 2.0f};
				cellPos += glm::vec2{(float)x, -((float)y)} * Game::cellSize;
				if (game.grid.progress[y][x] < 0) {
					draw_shape(cellPos, gscale, cross, this_color);
				}
				else {
					draw_shape(cellPos, gscale, square, this_color);
				}
			}
		}

		for (auto const &player : game.players) {
			glm::u8vec4 cur_color = glm::u8vec4(player.color * 255.0f, 0xff);
			float pscale = 0.5f;
			if (!player.fill_mode) {
				draw_shape(player.position, pscale, cross, cur_color);
			}
			else {
				draw_shape(player.position, pscale, square, cur_color);
			}

			if (&player == &game.players.front()) {
				// player indicator
				glm::vec2 indicator{Game::ArenaMin.x - 0.25f - game.clues.width * 0.05f, 0.0f};
				draw_shape(indicator, 1.0f, square, cur_color);
				draw_text(indicator + glm::vec2{0.08f, 0.0f} + glm::vec2(0.0f, -0.1f + Game::PlayerRadius), "You", 0.09f);
			}
			draw_shape(player.position, 1.0f, square, cur_color);
		}
	}
	GL_ERRORS();
}
