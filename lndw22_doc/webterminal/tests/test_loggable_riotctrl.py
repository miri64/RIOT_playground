# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.


from .. import loggable_riotctrl


def test_singleton():
    class TestClass(metaclass=loggable_riotctrl.Singleton):
        pass

    assert TestClass() is TestClass()


def test_riot_ctrl(tmpdir, mocker):
    file = tmpdir.join("test.log")
    mocker.patch(
        "webterminal.loggable_riotctrl.RIOTCtrl.make_command",
        lambda *args, **kwargs: ["tests/terminal_mock.py"],
    )
    orig_start_term = mocker.patch("riotctrl.ctrl.RIOTCtrl.start_term")
    mocker.patch("time.sleep")
    ctrl = loggable_riotctrl.RIOTCtrl(application_directory=".", logfile=str(file))
    with ctrl.run_term(), open(ctrl.logfile) as logfile:
        assert logfile.read() == "This goes to stderr\nThis goes to stdout\n"
    orig_start_term.assert_not_called()
    # unsingleton
    del loggable_riotctrl.RIOTCtrl._instances[loggable_riotctrl.RIOTCtrl]
    del ctrl
    ctrl = loggable_riotctrl.RIOTCtrl(application_directory=".")
    ctrl.start_term(foobar="test")
    orig_start_term.assert_called_once_with(foobar="test")
