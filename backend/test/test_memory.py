import sys
import os
import json
from unittest.mock import MagicMock, patch, mock_open

# Mocking heavy dependencies before importing server
sys.modules['faster_whisper'] = MagicMock()
sys.modules['silero_vad'] = MagicMock()
sys.modules['llama_cpp'] = MagicMock()
sys.modules['torch'] = MagicMock()
sys.modules['scipy'] = MagicMock()
sys.modules['scipy.io'] = MagicMock()
sys.modules['scipy.io.wavfile'] = MagicMock()

import pytest

# Mocking environment for server import
with patch('os.path.exists', return_value=True), \
     patch('shutil.which', return_value='/usr/bin/piper'), \
     patch('concurrent.futures.ThreadPoolExecutor'), \
     patch('os.path.expanduser', return_value='/tmp/models'):
    from src import server

def test_sauvegarder_memoire():
    """Test that sauvegarder_memoire correctly writes to the file."""
    m = mock_open()
    with patch('src.server.open', m), \
         patch('json.dump') as mock_json_dump:

        test_contexte = "L'utilisateur est un expert en robotique."
        server.sauvegarder_memoire(test_contexte)

        # Verify open was called with the correct file and mode
        m.assert_called_once_with(server.MEMORY_FILE, 'w', encoding='utf-8')

        # Verify json.dump was called with correct data
        mock_json_dump.assert_called_once()
        args, kwargs = mock_json_dump.call_args
        assert args[0] == {"contexte_utilisateur": test_contexte}
        assert kwargs['ensure_ascii'] is False
        assert kwargs['indent'] == 4

def test_charger_memoire_exists():
    """Test charger_memoire when the file exists and is valid."""
    test_data = {"contexte_utilisateur": "Utilisateur test."}
    m = mock_open(read_data=json.dumps(test_data))

    with patch('os.path.exists', return_value=True), \
         patch('src.server.open', m):

        result = server.charger_memoire()
        assert result == "Utilisateur test."

def test_charger_memoire_not_exists():
    """Test charger_memoire when the file does not exist."""
    with patch('os.path.exists', return_value=False):
        result = server.charger_memoire()
        assert result == "L'utilisateur est un étudiant/ingénieur travaillant sur un ESP32."

def test_charger_memoire_corrupted():
    """Test charger_memoire when the file is corrupted."""
    m = mock_open(read_data="corrupted data")

    with patch('os.path.exists', return_value=True), \
         patch('src.server.open', m):

        result = server.charger_memoire()
        # Should return default value on exception
        assert result == "L'utilisateur est un étudiant/ingénieur travaillant sur un ESP32."

def test_charger_memoire_missing_key():
    """Test charger_memoire when the key is missing in JSON."""
    test_data = {"wrong_key": "some value"}
    m = mock_open(read_data=json.dumps(test_data))

    with patch('os.path.exists', return_value=True), \
         patch('src.server.open', m):

        result = server.charger_memoire()
        # Should return the secondary default in mem.get
        assert result == "Aucun contexte particulier."
