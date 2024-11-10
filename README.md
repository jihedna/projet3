# Serveur de Chat Multi-Utilisateurs en C

Ce projet implémente un serveur de chat en C permettant à plusieurs clients de se connecter et de communiquer en temps réel via des threads et des sockets.

## Fonctionnalités
- Connexion simultanée de plusieurs clients, chacun identifié par un pseudonyme.
- Diffusion de messages en temps réel à tous les clients connectés.
- Notifications de connexion et de déconnexion des utilisateurs.
- Filtrage de contenu pour masquer les mots interdits.
- Gestion des erreurs et prévention des débordements de tampon.

## Prérequis
- Compilateur GCC avec support pour les threads (`pthread`).
- Système compatible avec les sockets POSIX (Linux, macOS, etc.).

## Instructions de Compilation

Pour compiler le serveur, utilisez la commande suivante dans un terminal :

```bash
gcc -pthread -o server server.c
