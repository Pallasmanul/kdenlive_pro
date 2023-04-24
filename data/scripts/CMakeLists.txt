# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: jb@kdenlive.org

INSTALL(FILES 
    checkpackages.py
    otiointerface.py
    speech.py
    speechtotext.py
    whispertotext.py
    whispertosrt.py
    checkgpu.py

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts)
