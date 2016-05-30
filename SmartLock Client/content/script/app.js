//var connection = new WebSocket('ws://192.168.1.102:81/', ['arduino']);
//connection.onopen = function () {
//    //connection.send('Connect ' + new Date());
//};
//connection.onerror = function (error) {
//    console.log('WebSocket Error ', error);
//};
//connection.onmessage = function (e) {
//    //console.log('Server: ', e.data);
//    //connection.send('Request ' + new Date());
//};

function init() {
    var $body = $('body'),
        $container = $('#container'),
        $loader = $('.loader'),
        $panel = $('#panel'),
        $keypad = $('#keypad'),
        $pattern = $('#pattern'),
        $bg = $('.bg'),
        $settings = $('#settings'),
        $status = $('.status'),
        $action = $('.action'),
        $button = $('.big-button'),
        $indicator = $('.indicator.unlocked'),
        $icon = $('.big-button button'),
        clickEvent = window.ontouchend ? eventName = 'touchend' : eventName = 'click',
        connection,
        password;


    $body.addClass('init');
    $pattern.removeClass('hidden');

    connect(function (success, error) {
        if (success) {
            connection.close();
            //        $container.css('opacity', 0);
            $pattern.removeClass('hidden').addClass('fade-in');
            setTimeout(function () {
                //$container.css('opacity', 1);
                $loader.addClass('hidden');
                $body.removeClass('init').addClass('auth');

                var lock = new PatternLock(".pattern", {
                    patternVisible: true,
                    mapper: function (idx) {
                        return (idx % 9) + 1;
                    },
                    margin: $container.width() / 14,
                    radius: $container.width() / 14,
                    onDraw: function (pattern) {
                        auth(pattern, function (success, state) {
                            if (success) {
                                $('.pattern').addClass('patt-success');
                                setTimeout(function () {
                                    enable(state);
                                }, 500);
                            }
                            else {
                                lock.error();
                                setTimeout(function () {
                                    lock.reset();
                                }, 1500);
                            }
                        });
                    }
                });

                $(".patt-wrap").addClass("animate");
                setTimeout(function () {
                    $('#pattern span').addClass('fade-in');
                }, 200);

                //lock.checkForPattern('2586', function () {
                //    $(".patt-holder").addClass("patt-success");
                //    setTimeout(function () {
                //        enable();
                //    }, 600);
                //}, function () {
                //    setTimeout(function () {
                //        lock.reset();
                //    }, 1500)
                //});





                //var $numbers = $('.number'),
                //    $backButton = $('#button-back'),
                //    $okButton = $('#button-ok'),
                //    $passwordBox = $('#password input'),
                //    currentChar = 0;

                //$backButton.on(clickEvent, function (e) {

                //    e.preventDefault();
                //});

                //$okButton.on(clickEvent, function (e) {
                //    enable();

                //    e.preventDefault();
                //});
            }, 700);
        }
    });

    function enable(state) {
        //$container.css('opacity', 0);
        $pattern.removeClass('fade-in');
        $panel.removeClass('hidden');
        setTimeout(function () {
            $pattern.addClass('hidden');
            $body.removeClass('auth');
            //$container.css('opacity', 1)
        }, 1000);

        setState(state);

        $button.on(clickEvent, function (e) {
            if ($body.hasClass('locked')) {
                open(setState, function () {
                    $indicator.css('opacity', 1);
                });                
            }
            else if ($body.hasClass('unlocked')) {
                lock(setState, function () {
                    $indicator.css('opacity', 0);
                });                
            }
            e.preventDefault();
        });
    }

    function setState(state) {
        if (state <= 0) {
            $status.text('UNLOCKED');
            $action.text('Tap to lock, hold to open');
            $body.addClass('unlocked').removeClass('locked');
            $indicator.css('opacity', 1);
        }
        else {
            $status.text('LOCKED');
            $action.text('Hold to unlock');
            $body.addClass('locked').removeClass('unlocked');;
            $indicator.css('opacity', 0);
        }
    }

    function connect(onComplete) {
        connection = new WebSocket('ws://192.168.1.102:81/', ['arduino']);
        connection.onopen = function () {
            console.log('Connection Opened');
            if (onComplete) {
                onComplete(true);
            }
        };

        connection.onerror = function (error) {
            if (onComplete) {
                onComplete(false, error);
            }

            console.log('Error ' + error);
        };

        //connection.onmessage = function (e) {

        //    connection.onmessage = onMessage;
        //}

        connection.onclose = function () {
            console.log('Connection Closed');
        }
    }

    function auth(pattern, onComplete) {
        password = pattern;
        sendMessage('auth', function (response) {
            onComplete(JSON.parse(response.data).auth, JSON.parse(response.data).state);
        });
    }

    function getState(onComplete) {
        sendMessage('getState', function (response) {
            onComplete(JSON.parse(response.data).state);
        });
    }

    function lock(onComplete, onSuccess) {
        sendMessage('lock', function (response) {
            onComplete(JSON.parse(response.data).state);
        }, onSuccess);
    }

    function unlock(onComplete, onSuccess) {
        sendMessage('unlock', function (response) {
            onComplete(JSON.parse(response.data).state);
        }, onSuccess);
    }

    function open(onComplete, onSuccess) {
        sendMessage('open', function (response) {
            onComplete(JSON.parse(response.data).state);
        }, onSuccess);
    }


    function onMessage(e) {
        console.log('Message Received', e.data);
    }

    function sendMessage(operation, onComplete, onSuccess) {
        connection.close();
        connect(function (success, error) {
            if (success) {
                connection.send('{"operation": "' + operation + '", "password": "' + password + '"}');

                if (onSuccess) {
                    onSuccess();
                }

                connection.onmessage = function (e) {
                    if (onComplete) {
                        onComplete(e);
                    }
                    onMessage(e);
                    connection.onmessage = onMessage;

                    connection.close();
                }
            }
        });
    }
}