#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>

using namespace std;

// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1);
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dist(1, 10); // Intervalo para parar a música

class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        jogadores.clear();
        musica_parada = false;
        cadeiras = num_jogadores - 1;
        cadeira_sem.release(cadeiras); // Ajusta o semáforo com o número de cadeiras para a rodada
        cout << endl << "Iniciando rodada com " << num_jogadores << " jogadores e " << cadeiras << " cadeiras." << endl;
        cout << "A música está tocando... 🎵" << endl << endl;
    }

    void parar_musica() {
        musica_parada = true;
        cout << "> A música parou! Os jogadores estão tentando se sentar..." << endl << endl;
        music_cv.notify_all();
    }

    void eliminar_jogador(int jogador_id) {
        num_jogadores--;
        cout << "Jogador " << jogador_id << " não conseguiu uma cadeira e foi eliminado!" << endl;
    }

    int get_num_jogadores() const {
        return num_jogadores;
    }

    void adicionar_jogador(int id) {
        jogadores.push_back(id);
    }

    void remover_jogador(int id) {
        jogadores.erase(std::remove(jogadores.begin(), jogadores.end(), id), jogadores.end());
    }

    int get_jogadores() {
        return jogadores[0];
    }


private:
    vector<int> jogadores;
    int num_jogadores;
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo) {}

    void tentar_ocupar_cadeira() {

        if (cadeira_sem.try_acquire()) {
            cadeira_sem.acquire();
            jogo.adicionar_jogador(id);
            cout << "Jogador " << id << " conseguiu uma cadeira." << endl;
        } else {
            eliminado = true;
            jogo.eliminar_jogador(id);
            jogo.remover_jogador(id);
        }
    }

    void joga() {
        while (jogo.get_num_jogadores() > 1 && jogo_ativo) {
            unique_lock<mutex> lock(music_mutex);
            music_cv.wait(lock, [] { return musica_parada.load(); });

            if (eliminado) break;

            tentar_ocupar_cadeira();

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(4000));
        }
    }

private:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado = false;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        while (jogo.get_num_jogadores() > 1) {
            jogo.iniciar_rodada();

            int wait_time = dist(gen);
            std::this_thread::sleep_for(std::chrono::seconds(wait_time));
            jogo.parar_musica();

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        cout << endl << "Jogador " << jogo.get_jogadores() << " é o vencedor!" << endl;
        cout << "Temos um vencedor!" << endl;
        jogo_ativo = false;
        music_cv.notify_all();
    }

private:
    JogoDasCadeiras& jogo;
};

// Main function
int main() {
    cout << "-----------------------------------------------" << endl;
    cout << "Bem-vindo ao Jogo das Cadeiras Concorrente!" << endl;
    cout << "-----------------------------------------------" << endl;

    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;

    // Criação das threads dos jogadores
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    // Esperar pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Esperar pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}
