import sqlite3
import tkinter as tk
from tkinter import messagebox

# Configurações
TAMANHO_SERIAL = 12  # Número exato de caracteres do serial
PREFIXO_VALIDO = ("00", "24")  # Prefixo necessário para o serial

# Criar banco de dados e tabela, se não existirem
def inicializar_banco():
    conn = sqlite3.connect("seriais.db")
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS seriais (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            serial TEXT UNIQUE NOT NULL
        )
    """)
    conn.commit()
    conn.close()

def verificar_serial_sqlite(serial):
    """Verifica se um serial já existe no banco de dados."""
    conn = sqlite3.connect("seriais.db")
    cursor = conn.cursor()
    
    cursor.execute("SELECT serial FROM seriais WHERE serial = ?", (serial,))
    resultado = cursor.fetchone()

    conn.close()
    return resultado[0] if resultado else None

def salvar_serial_sqlite(serial):
    """Salva um novo serial no banco de dados."""
    conn = sqlite3.connect("seriais.db")
    cursor = conn.cursor()

    try:
        cursor.execute("INSERT INTO seriais (serial) VALUES (?)", (serial,))
        conn.commit()
    except sqlite3.IntegrityError:
        pass  # Caso o serial já exista, não adiciona novamente

    conn.close()

def verificar_serial():
    """Lida com a entrada do usuário, faz validação e exibe o resultado."""
    serial = entry_serial.get().strip()

    # Validação do tamanho do serial
    if len(serial) != TAMANHO_SERIAL:
        resultado_label.config(text="⚠ Verifique o serial bipado!", fg="blue")
        reset_input()
        return
    
    # Validação do prefixo do serial
    if not serial.startswith(PREFIXO_VALIDO):
        resultado_label.config(text="⚠ Verifique o serial bipado!", fg="blue")
        reset_input()
        return

    # Consultar no banco de dados
    serial_encontrado = verificar_serial_sqlite(serial)

    if serial_encontrado:
        resultado_label.config(text=f"❌ Serial duplicado: {serial_encontrado}", fg="red")
    else:
        resultado_label.config(text="✅ Serial ok!", fg="green")

    reset_input()

def reset_input():
    """Reseta a entrada do serial após pesquisa."""
    entry_serial.delete(0, tk.END)
    entry_serial.focus()

# Inicializar banco de dados e tabela
inicializar_banco()

# Criar a janela principal
root = tk.Tk()
root.title("Verificador de Seriais")
root.geometry("400x250")
root.resizable(False, False)

# Criar widgets
tk.Label(root, text="Digite o serial:", font=("Arial", 12)).pack(pady=10)

entry_serial = tk.Entry(root, font=("Arial", 12), width=30)
entry_serial.pack()
entry_serial.bind("<Return>", lambda event: verificar_serial())  # Permite pesquisa ao pressionar Enter

btn_verificar = tk.Button(root, text="Verificar", font=("Arial", 12), command=verificar_serial)
btn_verificar.pack(pady=10)

resultado_label = tk.Label(root, text="", font=("Arial", 12))
resultado_label.pack(pady=5)

# Focar automaticamente no campo de entrada
entry_serial.focus()

# Iniciar o loop do Tkinter
root.mainloop()
