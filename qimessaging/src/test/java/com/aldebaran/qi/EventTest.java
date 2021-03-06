/*
**  Copyright (C) 2015 Aldebaran Robotics
**  See COPYING for the license
*/
package com.aldebaran.qi;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.*;

public class EventTest {
    private boolean callbackCalled = false;
    private int callbackParam = 0;
    public AnyObject proxy = null;
    public AnyObject obj = null;
    public Session s = null;
    public Session client = null;
    public ServiceDirectory sd = null;

    @Before
    public void setUp() throws Exception {
        sd = new ServiceDirectory();
        s = new Session();
        client = new Session();

        // Get Service directory listening url.
        String url = sd.listenUrl();

        // Create new QiMessaging generic object
        DynamicObjectBuilder ob = new DynamicObjectBuilder();

        // Get instance of ReplyService
        QiService reply = new ReplyService();

        // Register event 'Fire'
        ob.advertiseSignal("fire::(i)");
        ob.advertiseSignal("bigFire::([({s((s(is))[(s(is))])})])");
        ob.advertiseMethod("reply::s(s)", reply, "Concatenate given argument with 'bim !'");
        ob.advertiseMethod("answer::s()", reply, "Return given argument");
        ob.advertiseMethod("add::i(iii)", reply, "Return sum of arguments");
        ob.advertiseMethod("info::(sib)(sib)", reply, "Return a tuple containing given arguments");
        ob.advertiseMethod("answer::i(i)", reply, "Return given parameter plus 1");
        ob.advertiseMethod("answerFloat::f(f)", reply, "Return given parameter plus 1");
        ob.advertiseMethod("answerBool::b(b)", reply, "Flip given parameter and return it");
        ob.advertiseMethod("abacus::{ib}({ib})", reply, "Flip all booleans in map");
        ob.advertiseMethod("echoFloatList::[m]([f])", reply, "Return the exact same list");
        ob.advertiseMethod("createObject::o()", reply, "Return a test object");

        // Connect session to Service Directory
        s.connect(url).sync();

        // Register service as serviceTest
        obj = ob.object();
        assertTrue("Service must be registered", s.registerService("serviceTest", obj) > 0);

        // Connect client session to service directory
        client.connect(url).sync();

        // Get a proxy to serviceTest
        proxy = client.service("serviceTest").get();
        assertNotNull(proxy);
    }

    @After
    public void tearDown() {
        obj = null;
        proxy = null;

        s.close();
        client.close();

        s = null;
        client = null;
        sd = null;
    }

    @Test
    public void testEvent() throws InterruptedException {

        @SuppressWarnings("unused")
        Object callback = new Object() {
            public void fireCallback(Integer i) {
                callbackCalled = true;
                callbackParam = i.intValue();
            }
        };

        long sid = 0;
        try {
            sid = proxy.connect("fire::(i)", "fireCallback::(i)", callback);
        } catch (Exception e) {
            fail("Connect to event must succeed : " + e.getMessage());
        }
        obj.post("fire", 42);

        Thread.sleep(100); // Give time for callback to be called.
        assertTrue("Event callback must have been called ", callbackCalled);
        assertTrue("Parameter value must be 42 (" + callbackParam + ")", callbackParam == 42);

        proxy.disconnect(sid);
        callbackCalled = false;
        obj.post("fire", 42);
        Thread.sleep(100);
        assertTrue("Event callback not called ", !callbackCalled);
    }

    public void testCallback(String s) {
        callbackCalled = true;
    }

    @Test
    public void testSessionOnDisconnected() throws InterruptedException {
        callbackCalled = false;
        client.onDisconnected("testCallback", this);
        client.close();
        Thread.sleep(100);
        assertTrue(callbackCalled);
    }

    @Test
    public void testSignal() throws InterruptedException {
        final AtomicInteger value = new AtomicInteger();
        QiSignalConnection connection = proxy.connect("fire", new QiSignalListener() {
            @Override
            public void onSignalReceived(Object... args) {
                int v = (Integer) args[0];
                value.set(v);
            }
        });
        connection.waitForDone();
        obj.post("fire", 42);
        Thread.sleep(100);
        assertEquals(42, value.get());
        obj.post("fire", 99);
        Thread.sleep(100);
        assertEquals(99, value.get());
        connection.disconnect().sync();
        obj.post("fire", 12);
        Thread.sleep(100);
        assertEquals(99, value.get());
    }

    @Test
    public void testSignalSlot() throws InterruptedException {
        final AtomicBoolean correct = new AtomicBoolean();
        Object callback = new Object() {
            @QiSlot
            public void onResult(int i, String s) {
                correct.set(i == 42 && "hello".equals(s));
            }
        };
        QiSignalConnection connection = proxy.connect("fire", callback, "onResult");
        connection.waitForDone();
        obj.post("fire", 42, "hello");
        Thread.sleep(100);
        assertTrue(correct.get());
        connection.disconnect();
    }

    @Test(expected = QiSlotException.class)
    public void testSignalAmbiguousSlot() {
        Object callback = new Object() {
            @QiSlot
            public void onResult(int i, String s) {
            }

            @QiSlot
            public void onResult() {
            }
        };
        proxy.connect("fire", callback, "onResult");
    }

    @Test
    public void testSignalNamedSlot() throws InterruptedException {
        final AtomicBoolean correct = new AtomicBoolean();
        Object callback = new Object() {
            @QiSlot("abc")
            public void onResult(int i, String s) {
                correct.set(i == 42 && "hello".equals(s));
            }

            @QiSlot("def")
            public void onResult() {
            }
        };
        QiSignalConnection connection = proxy.connect("fire", callback, "abc");
        connection.waitForDone();
        obj.post("fire", 42, "hello");
        Thread.sleep(100);
        assertTrue(correct.get());
        connection.disconnect();
    }

    @QiStruct
    static class X {
        @QiField(0)
        String s;
        @QiField(1)
        Tuple raw;
    }

    @QiStruct
    static class Y {
        @QiField(0)
        X head;
        @QiField(1)
        List<X> tail;
    }

    @QiStruct
    static class Z {
        @QiField(0)
        Map<String, Y> map;
    }

    @Test
    public void testSignalSlotAutoConversion() throws ExecutionException, TimeoutException {
        // final Promise<Z> zPromise = new Promise<>();
        final Promise<List<Z>> listPromise = new Promise<List<Z>>();
        Object callback = new Object() {
            @QiSlot
            public void onResult(List<Z> l) {
                listPromise.setValue(l);
            }
        };

        QiSignalConnection connection = proxy.connect("bigFire", callback, "onResult");
        connection.waitForDone();

        obj.post("bigFire", buildBigFireParams());
        List<Z> result = listPromise.getFuture().get(100, TimeUnit.MILLISECONDS);
        assertEquals(1, result.size());
        Z z = result.get(0);
        Y y = z.map.get("myKey");
        X x = y.head;
        assertEquals("x1", x.s);
        assertEquals(1, x.raw.get(0));
        assertEquals("one", x.raw.get(1));
    }

    private static List<Tuple> buildBigFireParams() {
        Tuple x = Tuple.of("x1", Tuple.of(1, "one"));
        List<Tuple> listX = new ArrayList<Tuple>();
        listX.add(x);
        Tuple y = Tuple.of(x, listX);
        Map<String, Tuple> map = new HashMap<String, Tuple>();
        map.put("myKey", y);
        Tuple z = Tuple.of(map);
        List<Tuple> result = new ArrayList<Tuple>();
        result.add(z);
        return result;
    }

    @Test
    public void testSessionConnectionListener() throws ExecutionException, InterruptedException {
        Session session = new Session();
        final AtomicBoolean connectedCalled = new AtomicBoolean();
        final AtomicBoolean disconnectedCalled = new AtomicBoolean();
        session.addConnectionListener(new Session.ConnectionListener() {
            @Override
            public void onConnected() {
                connectedCalled.set(true);
            }

            @Override
            public void onDisconnected(String reason) {
                disconnectedCalled.set(true);
            }
        });
        session.connect(sd.listenUrl()).get();
        session.close();
        Thread.sleep(100);
        assertTrue(connectedCalled.get());
        assertTrue(disconnectedCalled.get());
    }

    @Test
    public void testMultipleSessionConnectionListeners() throws ExecutionException, InterruptedException {
        Session session = new Session();
        final AtomicInteger connectedCount = new AtomicInteger();
        final AtomicInteger disconnectedCount = new AtomicInteger();
        class Counter implements Session.ConnectionListener {
            @Override
            public void onConnected() {
                connectedCount.incrementAndGet();
            }

            @Override
            public void onDisconnected(String reason) {
                disconnectedCount.incrementAndGet();
            }
        }

        Counter counter = new Counter();

        // 3 ConnectionListeners during connected
        session.addConnectionListener(new Counter());
        session.addConnectionListener(new Counter());
        session.addConnectionListener(counter);

        session.connect(sd.listenUrl()).get();

        // 2 ConnectionListeners during disconnected
        session.removeConnectionListener(counter);

        session.close();
        Thread.sleep(100);
        assertEquals(3, connectedCount.get());
        assertEquals(2, disconnectedCount.get());
    }
}
