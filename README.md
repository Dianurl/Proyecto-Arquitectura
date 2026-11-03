# Proyecto-Arquitectura
# Sistema de Control de Acceso y Seguridad – Arduino UNO R4 WiFi


Este proyecto consiste en un sistema de control de acceso que utiliza tarjetas RFID y un PIN de cuatro dígitos para autenticar usuarios. Incorpora un servomotor para controlar un cerrojo, una pantalla LCD para mostrar instrucciones, un buzzer para alertas y ocho LEDs para representar los estados del sistema.

## Manual de usuario

### 1. Requisitos para utilizar el sistema

Para utilizar el prototipo se necesita:

- Prototipo físico con todos sus componentes conectados.
- Computadora con puerto USB para alimentar el Arduino UNO R4 WiFi.
- Power bank para alimentar el servomotor.
- Tarjeta RFID registrada en el sistema.
- PIN de cuatro dígitos correspondiente a la tarjeta.

### 2. Encendido del sistema

1. Conectar el Arduino UNO R4 WiFi a la computadora mediante USB.
2. Conectar el cable de alimentación del servomotor al power bank.
3. Esperar a que el Arduino inicialice los componentes.
4. Verificar que la pantalla LCD muestre el siguiente mensaje:

```text
CONTROL ACCESO
PRESENTE TARJETA
```

Una vez que aparezca este mensaje, el sistema estará listo para utilizarse.

### 3. Ingreso mediante tarjeta RFID

1. Acercar una tarjeta RFID al lector RC522 ubicado en la parte frontal del prototipo.
2. El sistema leerá el identificador de la tarjeta y comprobará si está registrada.
3. Si la tarjeta es válida, se solicitará el ingreso del PIN.
4. Si la tarjeta no está registrada, se denegará el acceso y el sistema regresará al estado de espera.

### 4. Ingreso del PIN

Después de presentar una tarjeta válida:

1. Ingresar el PIN de cuatro dígitos mediante el teclado matricial.
2. La pantalla mostrará asteriscos para ocultar los números ingresados.
3. Presionar la tecla `#` para confirmar el PIN.
4. Si el PIN es correcto, el servomotor abrirá el cerrojo.
5. Si el PIN es incorrecto, el sistema permitirá intentarlo nuevamente.

**Importante:** El usuario dispone de un máximo de tres intentos incorrectos antes de que se active la alarma.

### 5. Funciones del teclado

| Tecla | Función |
|---|---|
| `0–9` | Ingresar los cuatro dígitos del PIN. |
| `*` | Borrar el PIN ingresado. |
| `#` | Confirmar el PIN. |
| `D` | Cerrar el cerrojo después de cerrar la puerta. |

### 6. Apertura y cierre de la puerta

**Para abrir:**

1. Presentar una tarjeta RFID autorizada.
2. Ingresar el PIN correspondiente.
3. Confirmar utilizando la tecla `#`.
4. Esperar a que el servomotor libere el cerrojo.
5. Se abre la puerta.

**Para cerrar:**

1. Cerrar físicamente la puerta.
2. Presionar la tecla `D` en el teclado.
3. El servomotor regresará a su posición de cierre y asegurará el cerrojo.
4. El sistema volverá a esperar una tarjeta RFID.

**Nota:** Es importante cerrar primero la puerta y después presionar `D` para evitar que el cerrojo interfiera con el cierre físico.

### 7. Intentos incorrectos y alarma

El sistema permite un máximo de tres intentos incorrectos de PIN.

- **Primer intento incorrecto:** se informa del error y se permite volver a ingresar el PIN.
- **Segundo intento incorrecto:** se informa nuevamente del error y se permite el último intento.
- **Tercer intento incorrecto:** se bloquea temporalmente el acceso y se activa la alarma.

La alarma permanece activa durante un período corto de tiempo y posteriormente se desactiva automáticamente.

Después de esto, el sistema regresa al estado inicial y solicita nuevamente una tarjeta RFID.

### 8. Indicadores LED

El sistema utiliza ocho LEDs controlados mediante el registro de desplazamiento 74HC595.

| LED | Estado del sistema |
|---|---|
| LED 1 | Inicio del sistema |
| LED 2 | Esperando tarjeta RFID |
| LED 3 | Validando tarjeta RFID |
| LED 4 | Esperando ingreso del PIN |
| LED 5 | Validando PIN |
| LED 6 | Acceso autorizado |
| LED 7 | Acceso denegado |
| LED 8 | Alarma activada |

Estos indicadores permiten conocer visualmente el estado actual del sistema.

### 9. Recomendaciones de uso

- Utilizar únicamente tarjetas RFID registradas.
- No compartir los PIN de acceso con usuarios no autorizados.
- Mantener conectadas ambas fuentes de alimentación durante el funcionamiento.
- Evitar desconectar el power bank mientras el servomotor está realizando un movimiento.
- No forzar manualmente el cerrojo cuando se encuentra cerrado.
- Evitar manipular las conexiones internas mientras el sistema está encendido.

### 10. Apagado del sistema

Para apagar el prototipo:

1. Verificar que la puerta esté cerrada y el cerrojo asegurado.
2. Desconectar el Arduino de la computadora.
3. Desconectar la alimentación del servomotor del power bank.

## Documentación y código fuente

El repositorio contiene los siguientes archivos principales:

- **`Codigo_ProyectoFinal_AC.ino`**: código fuente desarrollado para el Arduino UNO R4 WiFi.
- **`Proyecto Fase #2- Arquitectura.pdf`**: documentación técnica del proyecto, incluyendo arquitectura, conexiones, componentes y fotografías del prototipo.

## Integrantes

- Axel Guillermo Alvarado Taracena
- Diego Andres Diaz Estupiñan
