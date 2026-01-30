#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <iomanip>

using namespace std;

// Kolory ANSI
const string RESET = "\033[0m";
const string BOLD  = "\033[1m";
const string CYAN  = "\033[36m";
const string GREEN = "\033[32m";
const string RED   = "\033[31m";
const string YELLOW= "\033[33m";

// STALE URZADZENIE DLA ucom0
const char* PORT_MODEMU = "/dev/cuaU0";

void czyscEkran() {
    cout << "\033[2J\033[1;1H";
}

void naglowek() {
    cout << CYAN << BOLD;
    cout << "========================================" << endl;
    cout << "        NOKIA ASHA 503 SMS CENTER       " << endl;
    cout << "  System: OpenBSD | Port: " << PORT_MODEMU << endl;
    cout << "========================================" << RESET << endl;
}

map<string, string> zaladujKontakty(string nazwaPliku) {
    map<string, string> kontakty;
    ifstream plik(nazwaPliku);
    string linia;
    if (!plik.is_open()) return kontakty;
    while (getline(plik, linia)) {
        size_t poz = linia.find(':');
        if (poz != string::npos) {
            kontakty[linia.substr(0, poz)] = linia.substr(poz + 1);
        }
    }
    return kontakty;
}

int skonfigurujPort(const char* sciezka) {
    // OpenBSD: O_RDWR (czytaj/pisz), O_NOCTTY (nie przejmuj terminala)
    int fd = open(sciezka, O_RDWR | O_NOCTTY);
    if (fd == -1) return -1;

    struct termios options;
    tcgetattr(fd, &options);
    
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    options.c_cflag |= (CLOCAL | CREAD | CS8);
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    
    // Tryb RAW - kluczowy dla OpenBSD, by nie interpretowalo znakow specjalnych
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    options.c_oflag &= ~OPOST;

    tcsetattr(fd, TCSANOW, &options);
    return fd;
}

void wyslijKomende(int fd, string komenda) {
    komenda += "\r";
    write(fd, komenda.c_str(), komenda.length());
    usleep(500000); 
}

int main() {
    map<string, string> kontakty = zaladujKontakty("kontakty.txt");
    
    czyscEkran();
    naglowek();

    int fd = skonfigurujPort(PORT_MODEMU);
    if (fd == -1) {
        cout << RED << "[!] BLAD: Nie mozna otworzyc " << PORT_MODEMU << RESET << endl;
        cout << "1. Sprawdz czy plik istnieje: ls -l " << PORT_MODEMU << endl;
        cout << "2. Sprawdz uprawnienia: doas usermod -G dialer " << getlogin() << endl;
        return 1;
    }

    cout << GREEN << "[*] Polaczono poprawnie z Nokia Asha 503" << RESET << endl;
    
    // Inicjalizacja modemu
    wyslijKomende(fd, "AT");
    wyslijKomende(fd, "AT+CMGF=1");

    while (true) {
        string odbiorca, tresc;
        cout << "\n" << BOLD << "ADRESAT (Nazwa/Numer lub 'exit'): " << RESET;
        cin >> odbiorca;
        if (odbiorca == "exit") break;

        string numer = (kontakty.count(odbiorca)) ? kontakty[odbiorca] : odbiorca;
        
        cout << CYAN << "DO: " << BOLD << numer << RESET << endl;
        cout << BOLD << "TRESC: " << RESET;
        cin.ignore();
        getline(cin, tresc);

        cout << YELLOW << "Wysylanie..." << RESET << "\r" << flush;

        string cmd = "AT+CMGS=\"" + numer + "\"";
        wyslijKomende(fd, cmd);
        
        write(fd, tresc.c_str(), tresc.length());
        char ctrlZ = 26; 
        write(fd, &ctrlZ, 1);

        sleep(2); 
        cout << GREEN << "[ OK ] Wiadomosc wyslana do: " << numer << "          " << RESET << endl;
    }

    close(fd);
    return 0;
}