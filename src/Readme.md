# Usage

## Constructing the Network-Driver
1) Create a <b>EEL_Device</b> object<br><code>EEL_Device* device = new EEL_Device(...);</code><br><br>
2) Pass the EEL_Device object to the new <b>EEL_Driver</b> object to be created<br><code>EEL_Driver* nd = new EEL_Driver(..., device);</code><br><br>
3) Since created object is a <b>pointer</b> you have to access the methods with <b>-></b><br><code>nd->run();</code><br><br>

## Creating a Port
It's quite simple, you have to specify a port number and it's buffer's size. Be wary that maximum buffer size is 255! (8-bit value holds the size.)<br><code>nd->set_port(22, 128);</code><br><br>

## Reading a Port
First you have to check if there is a message came in<br><code>if(nd->is_port_written(22)){...}</code><br><br>
Then you can "read" the message<br><code>nd->read_port(22, NULL, NULL)</code><br>You can pass <b>unsigned char pointer</b>s to retrieve the message's length and source port as well!<br><br>
Beware that it simply returns the port's buffer. So, don't <code>free()</code> it. Also when you read a port it will clear the <b>PORT_WRITE_FLAG</b> (you won't be able to read it anymore), so you might want to copy it to an another buffer.<br><br>


## Sending a Datagram
1) Create a <b>Datagram</b> from array of <b>data</b><br><code>Datagram* d = CreateDatagram(src_port, dest_port, data, data_length);</code><br><br>
2) After you have constructed the network driver, call <b>SendDatagram()</b><br><code>nd->SendDatagram(dest_addr, priority, <b>d</b>);</code><br><br>
If no such route exists, a routing message will be sent instead of your datagram and corresponding error code will be returned by the method.<br>
After you have successfully sent your datagram, you can clear it with <code>FreeDatagram(&d)</code>. Note that, it takes a <b>double pointer</b>.
### Optional Way
You can also call <b>SendDatagramBuffer()</b>, this function will buffer the datagram and will be sent when <code>nd->run()</code> is called. (However this method is not tested.)<br><br>

### Priorities
Currently there are 3 levels of priority. Namely,<br>
1)<b>LOW_PR</b>: Lowest priority - 0<br>
2)<b>MEDIUM_PR</b> or <b>NORMAL</b>: Medium priority - 1<br>
3)<b>HIGH_PR</b>: High priority - 2<br>
Use them wth <b>ToS::</b> prefix for not making a confusion<br>

## Requesting a Route Beforehand
There is also a function you can call before trying to send any data, namely <b>request_route()</b>.You can call it from an EEL_Driver object,<br><code>nd->request_route(dest_addr, REASON_NULL);</code><br><br>

## Checking Status of a Route
You can check it with the method,<br>
<code>nd->route_status(dest_addr);</code><br><br>
If the return value is not <b>ROUTE_ACTIVE</b>, you have to request the route again.<br><br>

## Running the Driver
It's easy: <code>nd->run()</code> starts the driver. Just make sure it runs (<code>nd->run()</code>) inside the <code>loop()</code> function. If you don't, network will not receive any messages, hence not be able to send any messages too!<br><br>
