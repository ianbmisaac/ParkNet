# Conectar Parking S2 con Firebase

La página ya incluye inicio de sesión con Google o correo y contraseña, consulta de disponibilidad en tiempo real, reservas por bloques de una hora y cancelación de reservas.

La configuración web de ParkNet ya está integrada en el HTML. Para habilitar registro y reservas:

1. En Authentication > **Método de acceso**, habilita **Correo electrónico/Contraseña** y **Google**. Para Google, elige el correo de asistencia del proyecto y guarda.
2. Crea la base de datos de Cloud Firestore.
3. Copia el contenido de [firestore.rules](../firebase/firestore.rules) en la pestaña **Reglas** de Firestore y publícalo.
4. Abre la página desde `http://localhost:5173` o Firebase Hosting. En Authentication > **Configuración** > **Dominios autorizados**, agrega `localhost` si aún no aparece. Si usas otro dominio, agrega ese dominio también.

La página usa la SDK web de Firebase desde el CDN oficial. La configuración web incluye una clave de cliente pública; no pegues aquí una clave de cuenta de servicio ni una contraseña de administrador.

## Ejecutar en local

Esta carpeta incluye un servidor local de Node sin dependencias adicionales. Desde la terminal de VS Code, en esta carpeta, ejecuta:

```sh
npm run dev
```

Abre `http://localhost:5173/` en el navegador. El HTML está dentro de `public/`. No necesitas ejecutar `npm install`; el proyecto utiliza módulos integrados de Node y Firebase se carga desde el CDN.

Al registrarse con correo, la página solicita a Firebase que envíe un correo de verificación; la cuenta puede reservar antes de confirmarlo. Google valida la cuenta durante el inicio de sesión, por lo que no se envía ese correo adicional.

## Datos que guarda

- Los bloqueos de horario se guardan en slots/{espacio}_{fecha}_{hora}. Solo contienen el espacio, la fecha, la hora y el identificador de la reserva.
- Cada persona puede leer sus reservas en users/{uid}/reservations/{bookingId}.
- Crear una reserva crea el bloqueo y el registro privado en una transacción. Si otra persona ya tomó el horario, Firestore rechaza la reserva.
- Cancelar una reserva elimina el bloqueo y marca el registro como cancelado en una transacción.

Las reservas duran una hora y se asignan a horas en punto. La aplicación no incluye todavía cobros ni un panel administrativo.

## Documentación oficial

- [Añadir Firebase a una aplicación web](https://firebase.google.com/docs/web/setup)
- [Autenticación con correo y contraseña](https://firebase.google.com/docs/auth/web/password-auth)
- [Inicio de sesión con Google](https://firebase.google.com/docs/auth/web/google-signin)
- [Reglas de seguridad de Cloud Firestore](https://firebase.google.com/docs/firestore/security/rules-conditions)
- [Transacciones de Firestore](https://firebase.google.com/docs/firestore/manage-data/transactions)
