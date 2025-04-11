import sqlite3

def criar_banco():
    """Cria um banco de dados SQLite e insere os seriais do arquivo."""
    conn = sqlite3.connect("seriais.db")
    cursor = conn.cursor()

    cursor.execute("CREATE TABLE IF NOT EXISTS seriais (id INTEGER PRIMARY KEY, serial TEXT UNIQUE)")

    with open("seriais.txt", "r", encoding="utf-8") as f:
        for line in f:
            serial = line.strip()
            cursor.execute("INSERT OR IGNORE INTO seriais (serial) VALUES (?)", (serial,))

    conn.commit()
    conn.close()

# Criar banco e carregar seriais
criar_banco()
