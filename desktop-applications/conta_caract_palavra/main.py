def contar_letras_espacos(nome_arquivo):
    try:
        with open(nome_arquivo, 'r', encoding='utf-8') as arquivo:
            conteudo = arquivo.read()
            letras = sum(c.isalpha() for c in conteudo)
            espacos = conteudo.count(' ')
            palavras = espacos + 1
            print(f"Total de letras: {letras}")
            print(f"Total de palavras: {palavras}")
    except FileNotFoundError:
        print(f"Arquivo '{nome_arquivo}' não encontrado.")
    except Exception as e:
        print(f"Ocorreu um erro: {e}")

# Exemplo de uso
nome_do_arquivo = "desktop-applications\conta_caract_palavra\exemplo.txt"
contar_letras_espacos(nome_do_arquivo)
